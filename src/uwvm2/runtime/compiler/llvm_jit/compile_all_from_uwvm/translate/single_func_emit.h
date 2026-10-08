// This header owns the single-function LLVM JIT lowering path for validated Wasm MVP bytecode.
//
// The implementation is intentionally header-only because the opcode case files included near the end of this file
// depend on the local lambdas and type aliases declared by the instruction dispatcher.  Keep the following maintenance
// model in mind when changing this file:
//   * Runtime storage and validation have already established the Wasm-level shape of the function, but the JIT still
//     performs defensive null, bounds, and type checks before creating LLVM IR.
//   * Direct LLVM IR is preferred for same-module Wasm calls, SIMD operations, and stable/pinned native memory accesses.
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

// LLVM IR bitcast alone is insufficient if a native bridge returns float/double:
// i386 ST0 can quiet an sNaN, including with SSE2 arithmetic and at GCC -O0.
// The provider implementation, function-pointer signature, emitted direct path
// and bytecode-fallback path must agree on i32/i64 carriers. Bitcast only inside
// generated IR; these are bit representations, never numerical FP conversions.
// Native provider callbacks copy scalar bits. Use integer returns/arguments so
// neither a legacy FP ABI nor an x87 register can quiet an sNaN in transit.
[[nodiscard]] inline ::llvm::Type* get_llvm_jit_scalar_bits_type(::llvm::Type* type) noexcept
{
    return type->isFloatTy() ? ::llvm::Type::getInt32Ty(type->getContext()) :
           type->isDoubleTy() ? ::llvm::Type::getInt64Ty(type->getContext()) : type;
}

struct llvm_jit_stack_value_t
{
    // Wasm scalar type represented by `value`.
    runtime_operand_stack_value_type type{};

    // LLVM SSA value for the operand.  Null means the stack entry is unusable and should make the emitter fail.
    ::llvm::Value* value{};

    // Only a value emitted by ref.func carries this exact, VM-validated origin. A later local/table/select
    // result defaults to unknown, so dynamic references still pass through the runtime identity/type bridge.
    ::std::size_t known_ref_func_index{(::std::numeric_limits<::std::size_t>::max)()};

    // A compiler-private result of an immediately preceding checked local
    // immutable numeric struct cast. This is NOT the Wasm reference payload.
    // Consume only in the same still-empty success block: any generated call,
    // allocation, snapshot, mutation, debug point or control transfer ends the
    // borrow. No local/PHI/table/global/result stores propagate this witness.
    ::llvm::Value* immutable_gc_values_witness{};
    ::llvm::BasicBlock* immutable_gc_values_witness_block{};
    ::std::uint_least32_t immutable_gc_values_witness_type{};
    ::std::size_t immutable_gc_values_witness_next_offset{SIZE_MAX};
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

    // The emitted runtime/host call site. The shared buffer/result path also
    // accepts an invoke supplied by its caller; existing emitters still create calls.
    ::llvm::CallBase* bridge_call{};

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

// Size snapshot used for imported memories whose current byte length is available only through a bridge call.
struct llvm_jit_memory_snapshot_values_t
{
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

// References use opaque integers; v128 stays a byte vector throughout SSA, locals, PHIs and Wasm-to-Wasm calls.
// Using i128 for vector locals makes LLVM split loop-carried values into GPRs even when every hot operation is SIMD.
// C++/provider boundaries still use raw byte buffers, so no native-vector C++ ABI is exposed by this representation.
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
            return ::llvm::FixedVectorType::get(::llvm::Type::getInt8Ty(llvm_context), 16u);
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::funcref)):
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::externref)):
        case 0x69u: // Core 3 exnref uses the same opaque tagged-reference carrier.
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

#include "host_address_emit.h"
#include "relocatable_host_symbol_emit.h"

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
[[nodiscard]] inline constexpr ::llvm::Value* get_llvm_runtime_bridge_function_symbol_value_unwrapped(::llvm::IRBuilder<>& ir_builder,
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

    auto symbol_name{get_llvm_runtime_bridge_function_symbol_name<Function>(function_type, discriminator)};
    auto symbol_name_ref{get_llvm_string_ref(symbol_name)};
    auto function{llvm_module->getFunction(symbol_name_ref)};
    if(function != nullptr)
    {
        if(function->getFunctionType() != function_type) [[unlikely]] { return nullptr; }
    }
    else
    {
        function = ::llvm::Function::Create(function_type, ::llvm::Function::ExternalLinkage, symbol_name_ref, llvm_module);
    }
    ::llvm::sys::DynamicLibrary::AddSymbol(symbol_name_ref, reinterpret_cast<void*>(function_address));
#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
    // The nearby pointer carrier uses R_RISCV_64 to rebind a far bridge on
    // every materialization. No current-process address enters the object.
    return get_llvm_relocatable_host_symbol_pointer(ir_builder, function);
#else
    return function;
#endif
}

#include "single_func_debug_host_bridge.h"

// Selection is a compiler decision from the actual debug-full emit state. It
// adds no runtime flag/TLS probe to ordinary memory/global/provider functions.
// Do not scope entry/retirement/poll/diagnostic bridges: a before-park capture
// must observe the live generated ledger rather than an invented host island.
template <auto Function>
[[nodiscard]] inline constexpr ::llvm::Value* get_llvm_runtime_bridge_function_symbol_value(::llvm::IRBuilder<>& ir_builder,
    ::llvm::FunctionType* function_type, ::uwvm2::utils::container::u8string_view discriminator = {}) noexcept
{
    // [LLVM function/module-owned insertion point] node_end
    // [safe                                      ] borrowed only during emission;
    //  ^^ neither handle is a guest/native-stack address and no cursor advances.
    auto const block{ir_builder.GetInsertBlock()};
    auto const function{block == nullptr ? nullptr : block->getParent()};
    if constexpr(!llvm_jit_debug_control_bridge<Function>)
    {
        if(function != nullptr && function->hasFnAttribute("uwvm.debug.activation"))
        {
            if constexpr(requires { llvm_jit_debug_host_bridge_wrapper<Function>::invoke; })
            {
                return get_llvm_runtime_bridge_function_symbol_value_unwrapped<
                    llvm_jit_debug_host_bridge_wrapper<Function>::invoke>(ir_builder, function_type, discriminator);
            }
            else { return nullptr; } // unsupported native ABI cannot claim a complete debug activation chain.
        }
    }
    return get_llvm_runtime_bridge_function_symbol_value_unwrapped<Function>(ir_builder, function_type, discriminator);
}

[[nodiscard]] inline constexpr ::llvm::Value* get_llvm_external_host_object_pointer(::llvm::IRBuilder<>& ir_builder,
                                                                                    ::std::uintptr_t host_address,
                                                                                    ::llvm::Type* object_type,
                                                                                    ::uwvm2::utils::container::u8string_view symbol_name) noexcept
{
    if(host_address == 0u || object_type == nullptr || symbol_name.empty()) [[unlikely]] { return nullptr; }

    auto pointer_type{get_llvm_pointer_type(object_type)};
    if(pointer_type == nullptr) [[unlikely]] { return nullptr; }

    // Native off-world cache-OFF emission is selected only by the private
    // nonmoving engine factory's genuine staged source record and exact LLVM
    // module. Never register clone-owned mutable object names globally.
    auto const insertion_block{ir_builder.GetInsertBlock()};
    auto const insertion_function{insertion_block == nullptr ? nullptr : insertion_block->getParent()};
    auto const insertion_module{insertion_function == nullptr ? nullptr : insertion_function->getParent()};
    switch(private_host_object_emission_scope::select(insertion_module))
    {
        case private_host_object_emission_scope::selection::wrong_module: return nullptr;
        case private_host_object_emission_scope::selection::owned_address:
            return get_llvm_host_pointer_value(ir_builder, host_address, pointer_type);
        case private_host_object_emission_scope::selection::ordinary: break;
    }

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
#if defined(__i386__) || defined(_M_IX86) || defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || \
    (defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64))
        return get_llvm_relocatable_host_symbol_pointer(ir_builder, existing);
#else
        return existing;
#endif
    }
    auto global_value{llvm_module->getOrInsertGlobal(symbol_name_ref, object_type)};
    auto global{::llvm::dyn_cast<::llvm::GlobalVariable>(global_value)};
    if(global == nullptr) [[unlikely]] { return nullptr; }
    global->setLinkage(::llvm::GlobalValue::ExternalLinkage);
    global->setInitializer(nullptr);
    ::llvm::sys::DynamicLibrary::AddSymbol(symbol_name_ref, reinterpret_cast<void*>(host_address));
#if defined(__i386__) || defined(_M_IX86) || defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || \
    (defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64))
    // The target only addresses nearby JIT storage. Its pointer initializer
    // relocates to this exact external object again on a cache hit.
    return get_llvm_relocatable_host_symbol_pointer(ir_builder, global);
#else
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
    // Preserve ordinary Wasm calls for this runtime's recursive trap traces and native-unwind/address mapping policy.
    // A profitable native sibling call must not erase a caller that those diagnostics expect to reconstruct. This is
    // our runtime policy, not a claim that the Wasm specification mandates a physical native frame for every call.
    // Explicit Wasm tail-call proposal opcodes have different semantics and need their own implementation/policy.
    // LLVM exposes this as a string-valued semantic attribute rather than a stable enum attribute on the Function API.
    function.addFnAttr(get_llvm_string_ref(u8"disable-tail-calls"), get_llvm_string_ref(u8"true"));

    // LLVM execution enters with FE_DFL_ENV, and both scalar formats require gradual underflow. State this explicitly
    // so target lowering cannot inherit a flush-to-zero denormal policy from ambient toolchain defaults.
    function.addFnAttr(get_llvm_string_ref(u8"denormal-fp-math"), get_llvm_string_ref(u8"ieee,ieee"));
    function.addFnAttr(get_llvm_string_ref(u8"denormal-fp-math-f32"), get_llvm_string_ref(u8"ieee,ieee"));

    // A large locals/spill frame must touch each page before crossing the OS
    // stack guard. These backends implement LLVM inline stack probes; do not
    // replace Windows' platform-specific __chkstk lowering or pretend that
    // an ignored attribute protects other targets. Small frames gain no call.
    ::llvm::Triple target{function.getParent()->getTargetTriple()};
    if(target.getArch() == ::llvm::Triple::UnknownArch) { target = ::llvm::Triple{::llvm::sys::getDefaultTargetTriple()}; }
    if(!target.isOSWindows() &&
       (target.isX86() || target.isAArch64() || target.getArch() == ::llvm::Triple::systemz
#if LLVM_VERSION_MAJOR >= 20
        || target.isRISCV()
#endif
        ))
    {
        function.addFnAttr("probe-stack", "inline-asm");
        // RISC-V splits off up to 2048 - StackAlign bytes for CSR saves,
        // which can touch near the old SP rather than the adjusted SP.
        // A subsequent 4096-byte probe can therefore skip a 4 KiB guard.
        // Half-page probes bound that combined gap below 4096 bytes.
        auto const* probe_size{target.isRISCV() ? "2048" : "4096"};
#if defined(__APPLE__) && defined(__aarch64__)
        // Native Apple arm64 guards cannot be smaller than the physical OS
        // page. Use the proven 16-KiB interval only on that native platform;
        // non-Mac targets, iOS, unknown pages and other hosts retain 4 KiB.
        // This is queried while emitting IR, never by generated Wasm code.
        if(target.getArch() == ::llvm::Triple::aarch64 && target.isMacOSX())
        {
            auto const page{::uwvm2::object::memory::platform_page::get_platform_page_size()};
            if(page.success && page.page_size == 16384uz) { probe_size = "16384"; }
        }
#endif
        function.addFnAttr("stack-probe-size", probe_size);
    }
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
#if defined(__linux__) && defined(__powerpc64__) && defined(LLVM_UWVM_ROS_ELF_PPC32_MCJIT)
    // The paired backend forces normal global-entry calls to restore TOC;
    // only this exact private ABI admits changing TOC through C musttail.
    if(function.getCallingConv() == ::llvm::CallingConv::C && !function.isVarArg())
    { function.addFnAttr("uwvm-ppc64-toc-tail", "1"); }
#endif
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

// Preserve native unwind metadata for the native call-stack mode, which omits generated logical push/pop calls.
inline constexpr void apply_llvm_jit_unwind_call_stack_function_attrs(::llvm::Function& function) noexcept
{
    // Emit asynchronous unwind tables, not only call-site unwind info.  Wasm traps can be reported from arbitrary
    // instruction PCs after bounds checks, helper calls, or target signals/SEH faults, so the runtime unwinder needs CFI
    // that remains valid between calls when reconstructing optimized JIT frames.  On Win64 this causes LLVM to emit
    // .pdata/.xdata records; on DWARF targets it keeps CFI in .eh_frame. The matching runtime/host bridges must also
    // retain asynchronous CFI so a normal native walk can cross helpers and the OS signal trampoline back into Wasm.
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

inline constexpr void apply_llvm_jit_i386_fastcall_param_attrs(::llvm::CallBase& call_inst) noexcept
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
inline constexpr void apply_llvm_jit_calling_conv(::llvm::CallBase& call_inst, ::llvm::CallingConv::ID calling_conv) noexcept
{
    call_inst.setCallingConv(calling_conv);
#if defined(__i386__) || defined(_M_IX86)
    if(calling_conv == ::llvm::CallingConv::X86_FastCall) { apply_llvm_jit_i386_fastcall_param_attrs(call_inst); }
#endif
}

// Preserve the concrete call-site type: existing call builders retain CallInst*
// (including musttail setters), while invoke builders may retain InvokeInst*.
template <typename CallSite>
    requires ::std::derived_from<CallSite, ::llvm::CallBase>
inline constexpr CallSite* apply_llvm_jit_calling_conv(CallSite* call_inst, ::llvm::CallingConv::ID calling_conv) noexcept
{
    if(call_inst != nullptr) { apply_llvm_jit_calling_conv(*call_inst, calling_conv); }
    return call_inst;
}

// Mark a generated function as callable from the host/runtime bridge ABI.
inline constexpr void apply_llvm_jit_host_calling_conv(::llvm::Function& function) noexcept
{ apply_llvm_jit_calling_conv(function, get_llvm_jit_host_calling_conv()); }

// Mark a generated call site as crossing the host/runtime bridge ABI.
template <typename CallSite>
    requires ::std::derived_from<CallSite, ::llvm::CallBase>
inline constexpr CallSite* apply_llvm_jit_host_calling_conv(CallSite* call_inst) noexcept
{ return apply_llvm_jit_calling_conv(call_inst, get_llvm_jit_host_calling_conv()); }

// Raw Wasm entry targets are called both from generated code and from the C++ tiered runtime.  They deliberately share
// the private Wasm ABI above, so update the runtime-side entry pointer attributes whenever this convention changes.
[[nodiscard]] inline constexpr ::llvm::CallingConv::ID get_llvm_jit_raw_entry_calling_conv() noexcept { return get_llvm_jit_wasm_calling_conv(); }

// Mark a generated function as callable through the runtime raw-buffer ABI.
inline constexpr void apply_llvm_jit_raw_entry_calling_conv(::llvm::Function& function) noexcept
{ apply_llvm_jit_calling_conv(function, get_llvm_jit_raw_entry_calling_conv()); }

// Tail markers belong only to CallInst. Preserve the existing notail marker on
// ordinary calls without trying to attach one to an invoke or callbr. Concrete
// CallInst callers require no instruction-kind test, even before optimization.
template <typename CallSite>
    requires ::std::derived_from<CallSite, ::llvm::CallBase>
inline constexpr void apply_llvm_jit_no_tail_call_marker(CallSite* call_inst) noexcept
{
    if(call_inst == nullptr) { return; }
    if constexpr(::std::same_as<CallSite, ::llvm::CallInst>)
    {
        call_inst->setTailCallKind(::llvm::CallInst::TCK_NoTail);
    }
    else
    {
        if(auto const call{::llvm::dyn_cast<::llvm::CallInst>(call_inst)}; call != nullptr)
        {
            call->setTailCallKind(::llvm::CallInst::TCK_NoTail);
        }
    }
}

// Mark a generated raw-entry call site.
template <typename CallSite>
    requires ::std::derived_from<CallSite, ::llvm::CallBase>
inline constexpr CallSite* apply_llvm_jit_raw_entry_calling_conv(CallSite* call_inst) noexcept
{
    auto ret{apply_llvm_jit_calling_conv(call_inst, get_llvm_jit_raw_entry_calling_conv())};
    apply_llvm_jit_no_tail_call_marker(ret);
    return ret;
}

// LLVM tailcc permits musttail between different fixed parameter lists. Keep
// the C++ raw-entry ABI separate: only generated typed entries/calls use this
// convention. Qualified X86/AArch64/ARM providers and the explicitly patched
// RISC-V/LoongArch64 providers implement guaranteed tailcc argument-area reuse.
// External providers opt in with UWVM_LLVM_RISCV_TAILCC_FIXED=1 or
// UWVM_LLVM_LOONGARCH64_TAILCC_FIXED=1 after qualification. The bundled
// LoongArch64 provider advertises its repair in llvm-config.h; ROS checks that
// capability alongside its exact dependency revision.
// The qualified static Linux N64EL R2 provider uses the same callee-pop
// protocol; external backports opt in with UWVM_LLVM_MIPS_N64EL_R2_TAILCC_FIXED.
// Other MIPS ISA/ABI profiles retain their independently qualified convention.
// Aggregates can acquire hidden sret/byval storage, so keep their existing ABI
// unless an explicit scalar result-address operand was supplied.
[[nodiscard]] inline constexpr ::llvm::CallingConv::ID get_llvm_jit_typed_calling_conv(::llvm::FunctionType const& type) noexcept
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86) || defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM) || \
    (defined(__riscv) && defined(UWVM_LLVM_RISCV_TAILCC_FIXED) && UWVM_LLVM_RISCV_TAILCC_FIXED == 1) || \
    (defined(__loongarch_grlen) && __loongarch_grlen == 64 && \
     ((defined(UWVM_LLVM_LOONGARCH64_TAILCC_FIXED) && UWVM_LLVM_LOONGARCH64_TAILCC_FIXED == 1) || \
      (defined(LLVM_UWVM_ROS_LOONGARCH64_TAILCC) && LLVM_UWVM_ROS_LOONGARCH64_TAILCC == 1))) || \
    (defined(__linux__) && defined(__mips__) && defined(__mips64) && defined(__MIPSEL__) && __SIZEOF_POINTER__ == 8 && \
     defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips) && \
     ((defined(UWVM_LLVM_MIPS_N64EL_R2_TAILCC_FIXED) && UWVM_LLVM_MIPS_N64EL_R2_TAILCC_FIXED == 1) || \
      (defined(LLVM_UWVM_ROS_MIPS_N64EL_R2_TAILCC) && LLVM_UWVM_ROS_MIPS_N64EL_R2_TAILCC == 1)))
# if (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)) && \
     !((defined(LLVM_UWVM_X86_TAILCC_ALIGNED_FRAME_FIXED) && LLVM_UWVM_X86_TAILCC_ALIGNED_FRAME_FIXED == 1) || \
       (defined(UWVM_LLVM_X86_TAILCC_ALIGNED_FRAME_FIXED) && UWVM_LLVM_X86_TAILCC_ALIGNED_FRAME_FIXED == 1))
    // LLVM version numbers and the older X86 TailCC opt-in do not prove the
    // incoming/outgoing aligned-frame callee-pop repair. ROS revision 11 had
    // mismatched stack sizes for high-arity i128 carriers even with LLVM 23.1.1.
    // Require the generated repaired-provider capability or this separately
    // qualified backport opt-in. Unqualified providers retain the established
    // ABI; the existing musttail guards reject unsupported tail materialization.
    // Qualify the exact target/provider through the toolchain ABI regressions.
    static_cast<void>(type);
# else
    if(!type.getReturnType()->isAggregateType())
    {
        bool aggregates{};
        for(auto parameter: type.params()) { aggregates = aggregates || parameter->isAggregateType(); }
        if(!aggregates) { return ::llvm::CallingConv::Tail; }
    }
# endif
#endif
    return get_llvm_jit_wasm_calling_conv();
}

// Mark a generated function as callable through the private Wasm-to-Wasm ABI.
inline constexpr void apply_llvm_jit_wasm_calling_conv(::llvm::Function& function) noexcept
{ apply_llvm_jit_calling_conv(function, get_llvm_jit_typed_calling_conv(*function.getFunctionType())); }

// Mark a generated call site as using the private Wasm-to-Wasm ABI.
template <typename CallSite>
    requires ::std::derived_from<CallSite, ::llvm::CallBase>
inline constexpr CallSite* apply_llvm_jit_wasm_calling_conv(CallSite* call_inst) noexcept
{
    if(call_inst == nullptr) { return nullptr; }
    auto ret{apply_llvm_jit_calling_conv(call_inst, get_llvm_jit_typed_calling_conv(*call_inst->getFunctionType()))};
    apply_llvm_jit_no_tail_call_marker(ret);
    return ret;
}

// Return the byte size used by the raw bridge ABI for one Wasm scalar value.  These sizes intentionally match the
// parser's Wasm scalar storage types, not any native C++ promotion rules.
[[nodiscard]] inline constexpr ::std::size_t get_runtime_wasm_value_type_abi_size(runtime_operand_stack_value_type value_type) noexcept
{
    if(get_runtime_wasm_value_type_encoding(value_type) == 0x69u)
    { return sizeof(::uwvm2::object::global::wasm_global_ref_t); }
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
        case 0x69u: // Core 3 exnref is a tagged reference in LLVM SSA/local storage.
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::v128)):
            return true;
        default:
            return false;
    }
}

// The private LLVM Wasm-to-Wasm ABI carries v128 as a byte vector and references as opaque integers. Host/import boundaries continue
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
    // POSIX native mode uses an ordinary <unwind.h> walk from the runtime helper; no explicit frame-register seed is needed.
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
    if(!ir_builder.getCurrentDebugLocation())
    { trap_call->setMetadata("uwvm.generic.trap", ::llvm::MDNode::get(llvm_context, {})); }

    // Keep the trap call observable to LLVM optimizers even when surrounding code looks unreachable or side-effect-free.
    emit_llvm_jit_memory_clobber(ir_builder);
}

// Merge only compiler-tagged, constant-argument generic trap bodies after the
// complete function CFG exists. A wide reference loop otherwise gives LCSSA
// thousands of equivalent exits. Debug/provenance and target-specific register
// or carrier-load bodies keep their original blocks and source identity.
inline void coalesce_runtime_llvm_jit_generic_trap_blocks(::llvm::Function& function) noexcept
{
    ::llvm::DenseMap<::llvm::Value const*, ::llvm::SmallVector<::llvm::BasicBlock*, 8u>> canonical{};
    for(auto it{function.begin()}; it != function.end();)
    {
        auto& block{*it++};
        if(::std::addressof(block) == ::std::addressof(function.getEntryBlock()) ||
           block.hasAddressTaken() || block.size() != 3uz) { continue; }
        auto instruction{block.begin()};
        auto const call{::llvm::dyn_cast<::llvm::CallInst>(::std::addressof(*instruction++))};
        auto const barrier{::llvm::dyn_cast<::llvm::CallInst>(::std::addressof(*instruction++))};
        auto const terminal{::llvm::dyn_cast<::llvm::UnreachableInst>(::std::addressof(*instruction))};
        if(call == nullptr || barrier == nullptr || terminal == nullptr ||
           call->getMetadata("uwvm.generic.trap") == nullptr || call->isInlineAsm() ||
           !barrier->isInlineAsm() || !call->getType()->isVoidTy() ||
           call->getTailCallKind() != ::llvm::CallInst::TCK_NoTail ||
           call->arg_size() != 3u || call->getNumOperandBundles() != 0u ||
           !::llvm::isa<::llvm::Constant>(call->getCalledOperand()) ||
           call->getDebugLoc() || barrier->getDebugLoc() || terminal->getDebugLoc()) { continue; }
        bool constant_arguments{true};
        for(auto const& argument: call->args())
        { constant_arguments = constant_arguments && ::llvm::isa<::llvm::Constant>(argument.get()); }
        if(!constant_arguments) { continue; }
        auto& bucket{canonical[call->getCalledOperand()]};
        ::llvm::BasicBlock* equivalent{};
        for(auto const candidate: bucket)
        {
            auto previous{candidate->begin()};
            if(call->isIdenticalTo(::std::addressof(*previous++)) &&
               barrier->isIdenticalTo(::std::addressof(*previous)) &&
               terminal->isIdenticalTo(candidate->getTerminator()))
            { equivalent = candidate; break; }
        }
        if(equivalent == nullptr) { bucket.push_back(::std::addressof(block)); continue; }
        // Both blocks terminate, carry no PHIs, have no address-taken use and
        // return no SSA results. Redirect only identical cold control effects;
        // no allocation, root publication, cleanup or fast-path instruction moves.
        block.replaceAllUsesWith(equivalent);
        block.eraseFromParent();
    }
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

// Ordinary LLVM arithmetic may replace x / 1 or demote(promote(x)) with x, forwarding an sNaN.
// Wasm arithmetic must quiet it. Constrained native operations retain that observable instruction
// without adding a runtime helper or a compare/select sequence to every arithmetic operation.
// Extended-rounding targets retain ordinary IR for the dedicated software lowering pass.
struct llvm_wasm_arithmetic_scope
{
    ::llvm::IRBuilder<>& builder;
    bool constrained;
    ::llvm::RoundingMode rounding;
    ::llvm::fp::ExceptionBehavior exceptions;

    explicit llvm_wasm_arithmetic_scope(::llvm::IRBuilder<>& b) noexcept :
        builder{b}, constrained{b.getIsFPConstrained()}, rounding{b.getDefaultConstrainedRounding()},
        exceptions{b.getDefaultConstrainedExcept()}
    {
        if constexpr(!::uwvm2::runtime::compiler::shared::strict_float::needs_extended_rounding)
        {
            builder.setIsFPConstrained(true);
            builder.setDefaultConstrainedRounding(::llvm::RoundingMode::NearestTiesToEven);
            builder.setDefaultConstrainedExcept(::llvm::fp::ebStrict);
            builder.setConstrainedFPFunctionAttr();
        }
    }
    ~llvm_wasm_arithmetic_scope()
    {
        builder.setIsFPConstrained(constrained);
        builder.setDefaultConstrainedRounding(rounding);
        builder.setDefaultConstrainedExcept(exceptions);
    }
};

[[nodiscard]] inline ::llvm::Triple llvm_wasm_fp_target(::llvm::IRBuilder<>& builder) noexcept
{
    ::llvm::Triple target{builder.GetInsertBlock()->getModule()->getTargetTriple()};
    return target.getArch() == ::llvm::Triple::UnknownArch ? ::llvm::Triple{::llvm::sys::getDefaultTargetTriple()} : target;
}

[[nodiscard]] inline bool llvm_wasm_has_native_fp_feature(::llvm::IRBuilder<>& builder, ::llvm::StringRef name) noexcept
{
    auto const attribute{builder.GetInsertBlock()->getParent()->getFnAttribute("target-features")};
    if(attribute.isStringAttribute())
    {
        bool enabled{};
        ::llvm::SmallVector<::llvm::StringRef, 32> features;
        attribute.getValueAsString().split(features, ',');
        for(auto feature: features)
        {
            if(feature.size() > 1u && feature.drop_front() == name) { enabled = feature.front() == '+'; }
        }
        return enabled;
    }
    static auto const features{::llvm::sys::getHostCPUFeatures()};
    auto found{features.find(name)};
    return found != features.end() && found->second;
}

#include "int_to_float_emit.h"

// LoongArch's constrained FP lowering uses software libcalls even with native F/D hardware.
// Opaque native instructions retain sNaN quieting without the optimizer's identity folds.
[[nodiscard]] inline ::llvm::Value* emit_llvm_float_binary(::llvm::IRBuilder<>& builder, ::llvm::Value* lhs,
                                                          ::llvm::Value* rhs, unsigned operation) noexcept
{
    if(llvm_wasm_fp_target(builder).isLoongArch() &&
       llvm_wasm_has_native_fp_feature(builder, lhs->getType()->isFloatTy() ? "f" : "d"))
    {
        char const* const names[2][4]{{"fadd.s $0, $1, $2", "fsub.s $0, $1, $2", "fmul.s $0, $1, $2", "fdiv.s $0, $1, $2"},
                                     {"fadd.d $0, $1, $2", "fsub.d $0, $1, $2", "fmul.d $0, $1, $2", "fdiv.d $0, $1, $2"}};
        auto signature{::llvm::FunctionType::get(lhs->getType(), {lhs->getType(), rhs->getType()}, false)};
        auto instruction{::llvm::InlineAsm::get(signature, names[lhs->getType()->isFloatTy() ? 0u : 1u][operation], "=f,f,f", false)};
        return builder.CreateCall(signature, instruction, {lhs, rhs});
    }
    llvm_wasm_arithmetic_scope arithmetic_scope{builder};
    return operation == 0u ? builder.CreateFAdd(lhs, rhs) : operation == 1u ? builder.CreateFSub(lhs, rhs) :
           operation == 2u ? builder.CreateFMul(lhs, rhs) : builder.CreateFDiv(lhs, rhs);
}

[[nodiscard]] inline ::llvm::Value* emit_llvm_float_demote(::llvm::IRBuilder<>& builder, ::llvm::Value* value) noexcept
{
    if(llvm_wasm_fp_target(builder).isSPARC())
    {
        // LLVM 22 lowers even scalar constrained f64->f32 on hard-float SPARC
        // to __truncdfsf2, which its native libgcc does not provide. Ordinary
        // fptrunc lowers to fdtos (and lets backend CPU-erratum handling apply).
        // Wasm entry already establishes RN-even and masks FP exceptions. The
        // remaining observable strictness is NaN quieting: explicitly select a
        // canonical NaN from the INPUT bits, so folding demote(promote(sNaN))
        // cannot forward an sNaN and SPARC's native NaN encoding cannot leak.
        // Restrict the unconstrained scope to this conversion, not arithmetic
        // around it; soft-float targets still use their normal backend libcall.
        auto const constrained{builder.getIsFPConstrained()};
        builder.setIsFPConstrained(false);
        auto result{builder.CreateFPTrunc(value, builder.getFloatTy())};
        builder.setIsFPConstrained(constrained);
        auto raw{builder.CreateBitCast(value, builder.getInt64Ty())};
        auto magnitude{builder.CreateAnd(raw, builder.getInt64(0x7fffffffffffffffull))};
        auto nan{builder.CreateICmpUGT(magnitude, builder.getInt64(0x7ff0000000000000ull))};
        return builder.CreateSelect(nan, ::llvm::ConstantFP::getQNaN(builder.getFloatTy()), result);
    }
    if(llvm_wasm_fp_target(builder).isLoongArch() && llvm_wasm_has_native_fp_feature(builder, "d"))
    {
        auto signature{::llvm::FunctionType::get(builder.getFloatTy(), {builder.getDoubleTy()}, false)};
        auto instruction{::llvm::InlineAsm::get(signature, "fcvt.s.d $0, $1", "=f,f", false)};
        return builder.CreateCall(signature, instruction, {value});
    }
    llvm_wasm_arithmetic_scope arithmetic_scope{builder};
    return builder.CreateFPTrunc(value, builder.getFloatTy());
}

[[nodiscard]] inline ::llvm::Value* emit_llvm_float_promote(::llvm::IRBuilder<>& builder, ::llvm::Value* value) noexcept
{
    if(llvm_wasm_fp_target(builder).isLoongArch() && llvm_wasm_has_native_fp_feature(builder, "d"))
    {
        auto signature{::llvm::FunctionType::get(builder.getDoubleTy(), {builder.getFloatTy()}, false)};
        auto instruction{::llvm::InlineAsm::get(signature, "fcvt.d.s $0, $1", "=f,f", false)};
        return builder.CreateCall(signature, instruction, {value});
    }
    llvm_wasm_arithmetic_scope arithmetic_scope{builder};
    auto result{builder.CreateFPExt(value, builder.getDoubleTy())};
    ::llvm::Triple target{builder.GetInsertBlock()->getModule()->getTargetTriple()};
    if(target.getArch() == ::llvm::Triple::UnknownArch) { target = ::llvm::Triple{::llvm::sys::getDefaultTargetTriple()}; }
    if(target.isPPC())
    {
        // PPC lfs/xscvspdpn promotion preserves sNaNs. An exact native multiply quiets them;
        // constrained arithmetic prevents LLVM from folding this required operation away.
        result = builder.CreateFMul(result, ::llvm::ConstantFP::get(builder.getDoubleTy(), 1.0));
    }
    return result;
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
    get_runtime_imported_function_link_walk_bound(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
        ::uwvm2::uwvm::runtime::storage::runtime_registry_type const* compiler_registry = nullptr) noexcept
{
    auto bound{runtime_module.imported_function_vec_storage.size()};
    // The explicit owner is a cold metadata borrow, never a runtime selector.
    // Ordinary callers retain their existing actual initialized world.
    auto const& registry{compiler_registry == nullptr ?
        ::uwvm2::uwvm::runtime::storage::active_runtime_registry() : *compiler_registry};
    for(auto const& module_entry: registry)
    {
        auto const& other_module{module_entry.second};
        if(::std::addressof(other_module) == ::std::addressof(runtime_module)) { continue; }

        auto const imported_function_count{other_module.imported_function_vec_storage.size()};
        if(imported_function_count > ::std::numeric_limits<::std::size_t>::max() - bound) { return ::std::numeric_limits<::std::size_t>::max(); }
        bound += imported_function_count;
    }
    return bound;
}

enum class runtime_storage_pointer_membership : unsigned
{
    outside,
    element,
    invalid,
};

// Classify a possibly cross-module runtime pointer without relationally comparing or subtracting unrelated C++
// pointers.  An address outside this vector can legitimately name storage owned by the provider of an imported function or table;
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

// Follow imported-function forwarding records until the emitter can determine whether the target is a local defined
// function.  Non-local host/dynamic/weak targets are valid but not directly callable by typed LLVM IR.
[[nodiscard]] inline constexpr runtime_direct_callee_resolution_t
    resolve_runtime_direct_callee(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                  validation_module_traits_t::wasm_u32 func_index,
                                  ::uwvm2::uwvm::runtime::storage::runtime_registry_type const* compiler_registry = nullptr) noexcept
{
    using imported_function_storage_t = ::uwvm2::uwvm::runtime::storage::imported_function_storage_t;
    using function_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;

    runtime_direct_callee_resolution_t result{.state_valid = true};
    if(compiler_registry != nullptr)
    {
        bool member{};
        for(auto const& entry : *compiler_registry)
        { if(::std::addressof(entry.second) == ::std::addressof(runtime_module)) { member=true; break; } }
        if(!member) { return {}; } // comparison only; no foreign module read
    }
    auto const registered_import{[&](imported_function_storage_t const* candidate) constexpr noexcept
    {
        if(compiler_registry == nullptr) { return candidate != nullptr; }
        for(auto const& entry : *compiler_registry)
        {
            auto const& imports{entry.second.imported_function_vec_storage}; ::std::size_t index{};
            auto const membership{classify_runtime_storage_pointer(imports.data(), imports.size(), candidate, index)};
            if(membership == runtime_storage_pointer_membership::invalid) { return false; }
            if(membership == runtime_storage_pointer_membership::element) { return true; }
        }
        return false;
    }};
    auto const registered_defined{[&](::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const* candidate) constexpr noexcept
    {
        if(compiler_registry == nullptr) { return candidate != nullptr; }
        for(auto const& entry : *compiler_registry)
        {
            auto const& functions{entry.second.local_defined_function_vec_storage}; ::std::size_t index{};
            auto const membership{classify_runtime_storage_pointer(functions.data(), functions.size(), candidate, index)};
            if(membership == runtime_storage_pointer_membership::invalid) { return false; }
            if(membership == runtime_storage_pointer_membership::element) { return true; }
        }
        return false;
    }};

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
    auto const max_link_walk_steps{get_runtime_imported_function_link_walk_bound(runtime_module, compiler_registry)};
    for(::std::size_t steps{};; ++steps)
    {
        // Exceeding the storage-derived bound means the chain is no longer the acyclic structure the initializer proved.
        // [exact owned alias records in the selected compilation registry] end
        // [safe] explicit registry membership BEFORE any current record read.
        // Ordinary null-context aliases retain their initializer-owned proof.
        if(steps > max_link_walk_steps || !registered_import(curr)) [[unlikely]] { return {}; }

        switch(curr->link_kind)
        {
            case function_link_kind::imported:
            {
                // [checked current record] -> [comparison-only next record]
                // [safe] read the link only after current membership above.
                auto const* next{curr->target.imported_ptr};
                if(compiler_registry != nullptr && (steps == SIZE_MAX || !registered_import(next))) { return {}; }
                // [exact owned next record OR ordinary initializer-owned link]
                // [safe] explicit ownership BEFORE advancing the alias cursor;
                // a null ordinary link is rejected before its next dereference.
                curr = next;
                // [same proven next record] end; no Wasm byte pointer moves.
                continue;
            }
            case function_link_kind::defined:
            {
                auto defined_func_ptr{curr->target.defined_ptr};
                // [comparison-only final leaf] end
                // [safe] explicit registry element proof BEFORE the foreign
                // provider record read below; no initialized seal is invented.
                if(!registered_defined(defined_func_ptr)) [[unlikely]] { return {}; }

                ::std::size_t local_func_index{};
                // [current-module local storage, one-past) | provider module or malformed pointer
                // [safe exact element                    ] unsafe (foreign allocation or interior byte)
                // ^^ classify before dereference: an interior byte in this vector must fail without reading a partial record.
                auto const membership{classify_runtime_storage_pointer(local_func_begin, local_func_count,
                                                                       defined_func_ptr, local_func_index)};
                if(membership == runtime_storage_pointer_membership::invalid) [[unlikely]] { return {}; }
                // The initializer proves foreign aliases point to retained provider records; only an exact local element
                // or such a provider record is dereferenced below.
                result.function_type_ptr = defined_func_ptr->function_type_ptr;
                if(result.function_type_ptr == nullptr) [[unlikely]] { return {}; }
                if(membership == runtime_storage_pointer_membership::outside)
                {
                    // Retain the provider function type so the caller can use the raw ABI path.
                    return result;
                }
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
    }
    if(callee_function->getFunctionType() != callee_function_type) [[unlikely]] { return nullptr; }
#if defined(__linux__) && defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && \
    defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)
    // These names identify this module's generated definitions. They cannot
    // be preempted through ELF interposition. MIPS otherwise refuses even an
    // exact-prototype C musttail to an ordinary external-linkage declaration.
    // Imports and host symbols keep their existing binding contract.
    auto const index{static_cast<::std::size_t>(func_index)};
    auto const imports{runtime_module.imported_function_vec_storage.size()};
    if(index >= imports && index - imports < runtime_module.local_defined_function_vec_storage.size())
    { callee_function->setDSOLocal(true); }
#endif
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

// Multi-value entries use an explicit trailing result address and return void.
// The non-tail caller owns this packed buffer through the entire tail chain;
// forwarding its address never exposes storage in a retiring Wasm frame. LLVM
// musttail forbids automatic aggregate-to-sret conversion. Single values keep
// their register return ABI; scalar memory kernels gain no buffer or checks.
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
    if(llvm_result_type->isStructTy())
    {
        llvm_parameter_types.push_back(::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT)));
        llvm_result_type = ::llvm::Type::getVoidTy(llvm_context);
    }

    // Wasm function types have a fixed arity.  The final `false` explicitly disables LLVM varargs so verifier/type checks
    // catch any call-site arity mismatch instead of treating extra operands as native variadic arguments.
    return ::llvm::FunctionType::get(llvm_result_type, {llvm_parameter_types.data(), llvm_parameter_types.size()}, false);
}

// Exact runtime raw-call ABI signature. Every argument is register-wide uintptr_t/size_t, including the function index:
// some ABIs attach target-specific extension attributes to uint32_t parameters even when LLVM represents them as i32.
// Do not bind this handwritten FunctionType directly to pointer-typed or narrow-integer C++ implementations merely
// because a particular ABI happens to pass those values in the same registers.
[[nodiscard]] inline constexpr ::llvm::FunctionType* get_llvm_runtime_raw_call_bridge_function_type(::llvm::LLVMContext& llvm_context) noexcept
{
    static_assert(sizeof(::std::size_t) == sizeof(::std::uintptr_t),
                  "generated raw-call ABI represents size_t operands with LLVM intptr");
    static_assert(::std::numeric_limits<::std::size_t>::digits == ::std::numeric_limits<::std::uintptr_t>::digits,
                  "generated raw-call ABI requires size_t and uintptr_t to have the same value width");
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    return ::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context),
                                     {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type},
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
                                  ::llvm::StringRef result_buffer_name,
                                  ::llvm::Value* external_result_buffer_address = nullptr) noexcept
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
    if(external_result_buffer_address != nullptr)
    {
        if(external_result_buffer_address->getType() != llvm_intptr_type) { return {}; }
        // [non-tail caller's result_bytes ...] live through adapter return.
        // [safe                             ] already checked at the raw boundary.
        result_buffer_address = external_result_buffer_address;
    }
    else if(abi_layout.result_bytes != 0uz)
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

// Emit an ordinary typed call. Only multi-value signatures allocate a result
// buffer, in the entry block of the surviving caller (never on a tail edge).
// The call's result is reconstructed as SSA for the existing operand stack.
template<typename EmitCall>
[[nodiscard]] inline constexpr ::llvm::Value* emit_llvm_jit_typed_wasm_call(
    ::llvm::IRBuilder<>& builder,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_type,
    ::llvm::FunctionType* type, ::llvm::Value* callee, ::llvm::ArrayRef<::llvm::Value*> arguments, EmitCall&& emit_call) noexcept
{
    auto const layout{get_runtime_wasm_call_abi_layout(wasm_type)};
    if(!layout.valid || type == nullptr || callee == nullptr || arguments.size() != layout.parameter_count) { return nullptr; }
    if(layout.result_count <= 1uz) { return apply_llvm_jit_wasm_calling_conv(emit_call(type, callee, arguments)); }
    auto& context{builder.getContext()};
    auto const intptr{::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    // [caller-owned result_bytes ...] no Wasm-visible pointer, live until this
    // [safe                        ] normal call and all its tail targets return.
    auto const buffer{create_llvm_jit_entry_block_alloca(builder, ::llvm::Type::getInt8Ty(context),
        ::llvm::ConstantInt::get(intptr, layout.result_bytes), get_llvm_string_ref(u8"call.tuple.buffer"))};
    if(buffer == nullptr) { return nullptr; }
    auto const address{builder.CreatePtrToInt(buffer, intptr)};
    ::uwvm2::utils::container::vector<::llvm::Value*> operands{};
    operands.reserve(arguments.size() + 1uz);
    for(auto argument: arguments) { operands.push_back(argument); }
    operands.push_back(address);
    if(apply_llvm_jit_wasm_calling_conv(emit_call(type, callee, {operands.data(), operands.size()})) == nullptr) { return nullptr; }
    return emit_runtime_raw_call_result_value(builder, wasm_type,
        {.valid = true, .result_buffer = buffer, .result_buffer_address = address}, get_llvm_string_ref(u8"call.tuple.result"));
}

[[nodiscard]] inline constexpr ::llvm::Value* emit_llvm_jit_typed_wasm_call(
    ::llvm::IRBuilder<>& builder,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_type,
    ::llvm::FunctionType* type, ::llvm::Value* callee, ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept
{
    return emit_llvm_jit_typed_wasm_call(builder, wasm_type, type, callee, arguments,
        [&](::llvm::FunctionType* call_type, ::llvm::Value* target, ::llvm::ArrayRef<::llvm::Value*> operands) noexcept
        { return builder.CreateCall(call_type, target, operands); });
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

// Core 3 calls use the caller's immutable function-type forest preorder.
// Legacy modules retain the first equivalent signature index. Cold target
// publication uses the same namespace; neither path exposes a guest address.
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

    auto const* context{runtime_module.type_section_storage.core3_context_ptr};
    if(context != nullptr && !context->records.empty())
    {
        // [retained Core 3 type records, total][type_index_uz < total]
        // [safe                                                   ] no pointer or record cursor advances.
        // The caller preorder namespace is shared with cold target publication.
        // Aggregate placeholder signatures never identify a callable function.
        if(context->records.size() != total ||
           context->records.index_unchecked(type_index_uz).kind !=
               ::uwvm2::parser::wasm::standard::wasm3::type::composite_kind::function)
        { return invalid_runtime_canonical_type_id(); }
        auto const& record{context->records.index_unchecked(type_index_uz)};
        return record.preorder < UINT32_MAX && record.preorder < record.subtree_end &&
            record.subtree_end <= UINT32_MAX ? static_cast<::std::uint_least32_t>(record.preorder) :
            invalid_runtime_canonical_type_id();
    }
    auto const rich_begin{runtime_module.type_section_storage.owned_signature_begin};
    auto const rich_end{runtime_module.type_section_storage.owned_signature_end};
    // [owned_signature_begin ... owned_signature_end) is one initializer-retained array.
    // [safe                                         ] subtract only complete non-null endpoints.
    auto const rich_complete{rich_begin != nullptr && rich_end != nullptr && rich_end >= rich_begin &&
        static_cast<::std::size_t>(rich_end - rich_begin) == total};
    auto const rich_signatures{::uwvm2::validation::standard::wasm3::core3_signature_view<
        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{
        rich_complete ? rich_begin : nullptr, rich_complete ? total : 0uz}};
    auto const& target_type{type_begin[type_index_uz]};
    ::std::size_t canonical_index{type_index_uz};
    for(::std::size_t i{}; i != type_index_uz; ++i)
    {
        // The first equal type wins.  This lets call_indirect compare compact ids while preserving Wasm's structural type
        // equality rule for duplicate entries in the type section.
        if(runtime_wasm_function_types_equal(type_begin[i], target_type) &&
           (!rich_complete || ::uwvm2::validation::standard::wasm3::core3_function_types_equivalent(
               i, type_index_uz, rich_signatures)))
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

[[nodiscard]] inline constexpr ::std::uint_least32_t resolve_runtime_call_type_match_width(
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module,
    validation_module_traits_t::wasm_u32 type_index) noexcept
{
    auto const begin{resolve_runtime_canonical_type_id(module, type_index)};
    if(begin == invalid_runtime_canonical_type_id()) { return 0u; }
    auto const* context{module.type_section_storage.core3_context_ptr};
    if(context == nullptr || context->records.empty()) { return 1u; }
    // [immutable validated type forest][bounded index and interval]
    // [safe                                                      ] the resolver above proved
    // this exact record and an end <= UINT32_MAX; subtraction cannot wrap.
    auto const& record{context->records.index_unchecked(static_cast<::std::size_t>(type_index))};
    return static_cast<::std::uint_least32_t>(record.subtree_end - record.preorder);
}

[[nodiscard]] inline ::llvm::Value* emit_runtime_call_type_mismatch(
    ::llvm::IRBuilder<>& builder, ::llvm::Value* actual,
    ::std::uint_least32_t begin, ::std::uint_least32_t width) noexcept
{
    if(actual == nullptr || !actual->getType()->isIntegerTy(32u) ||
       begin == invalid_runtime_canonical_type_id() || width == 0u || width > UINT32_MAX - begin)
    { return nullptr; }
    // The sentinel UINT32_MAX lies outside every admitted interval. Singleton
    // types retain an equality check; polymorphic calls need one subtraction
    // and one unsigned comparison, without a load, lock or type walk.
    auto const first{::llvm::ConstantInt::get(actual->getType(), begin)};
    if(width == 1u) { return builder.CreateICmpNE(actual, first); }
    return builder.CreateICmpUGE(builder.CreateSub(actual, first),
        ::llvm::ConstantInt::get(actual->getType(), width));
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
                    // linked function import: [safe current record] -> [next record may be null]
                    // ^^ curr: the current record was checked; the next walk iteration rejects a null target. No Wasm byte pointer moves.
                    curr = curr->target.imported_ptr;
                    // [safe current record] -> [next record may be null]
                    //                          ^^ curr is checked before its next dereference.
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

// All union members start at the storage address. Use the C++ storage alignment, not the LLVM opaque-integer ABI
// alignment: a reference's native representation can be less aligned than the equal-width LLVM integer on some targets.
[[nodiscard]] inline constexpr ::llvm::Align get_llvm_global_storage_alignment() noexcept
{ return ::llvm::Align{alignof(::uwvm2::object::global::wasm_global_storage_u)}; }

// Build an LLVM external-symbol pointer to the active value field inside a directly addressable global storage record.
[[nodiscard]] inline constexpr ::llvm::Value* get_llvm_global_storage_pointer(::llvm::LLVMContext& llvm_context,
                                                                              ::llvm::IRBuilder<>& ir_builder,
                                                                              ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                                              validation_module_traits_t::wasm_u32 global_index,
                                                                              ::uwvm2::object::global::wasm_global_storage_t* global_storage_ptr,
                                                                              runtime_operand_stack_value_type value_type,
                                                                              bool engine_owned_pending = false) noexcept
{
    if(global_storage_ptr == nullptr) [[unlikely]] { return nullptr; }

    auto llvm_value_type{get_llvm_type_from_wasm_value_type(llvm_context, value_type)};
    if(llvm_value_type == nullptr) [[unlikely]] { return nullptr; }

    if(engine_owned_pending)
    {
        // Numeric pending bodies never publish generation-owned data through
        // DynamicLibrary::AddSymbol. Only an exact same-engine binding below
        // may resolve this declaration before native relocation/publication.
        if(!runtime_module.imported_global_vec_storage.empty()) { return nullptr; }
        auto const index{static_cast<::std::size_t>(global_index)};
        if(index >= runtime_module.local_defined_global_vec_storage.size()) { return nullptr; }
        // [canonical actual local-defined global vector] globals_end
        // [safe                                       ] unsafe (one-past)
        //  ^^ checked index selects this actual source's initialized record.
        auto const& record{runtime_module.local_defined_global_vec_storage.index_unchecked(index)};
        if(record.global_type_ptr == nullptr ||
           ::std::addressof(record.global) != global_storage_ptr ||
           static_cast<runtime_operand_stack_value_type>(record.global_type_ptr->type) != value_type)
        { return nullptr; }
        switch(value_type)
        {
            case runtime_operand_stack_value_type::i32:
            case runtime_operand_stack_value_type::i64:
            case runtime_operand_stack_value_type::f32:
            case runtime_operand_stack_value_type::f64:
            case runtime_operand_stack_value_type::v128: break;
            default: return nullptr;
        }
        auto const block{ir_builder.GetInsertBlock()};
        auto const module{block ? block->getModule() : nullptr};
        if(module == nullptr) { return nullptr; }
        auto const name{get_llvm_global_storage_symbol_name(runtime_module, global_index)};
        auto global{module->getNamedGlobal(get_llvm_string_ref(name))};
        if(global != nullptr)
        {
            if(!global->isDeclaration() || global->getValueType() != llvm_value_type ||
               global->getAddressSpace() != 0u || global->isConstant()) { return nullptr; }
        }
        else
        {
            // [complete current LLVM module][external exact numeric declaration]
            // [safe] host field addresses are absent from this generated IR.
            global = new ::llvm::GlobalVariable(*module, llvm_value_type, false,
                ::llvm::GlobalValue::ExternalLinkage, nullptr, get_llvm_string_ref(name));
        }
        global->setAlignment(get_llvm_global_storage_alignment());
        return global;
    }

    ::std::uintptr_t storage_address{};
    if(get_runtime_wasm_value_type_encoding(value_type) == 0x69u)
    {
        // exnref uses the same complete tagged carrier as other Wasm references.
        storage_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(global_storage_ptr->storage.ref));
    }
    else switch(value_type)
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
        case runtime_operand_stack_value_type::v128:
        {
            storage_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(global_storage_ptr->storage.v128));
            break;
        }
        case runtime_operand_stack_value_type::funcref:
        case runtime_operand_stack_value_type::externref:
        {
            // Keep the entire tagged reference payload. In particular, a cross-module funcref contains its resolved
            // owner identity; loading only a function index would incorrectly resolve it against the consuming module.
            storage_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(global_storage_ptr->storage.ref));
            break;
        }
        [[unlikely]] default:
        {
            return nullptr;
        }
    }

    auto const symbol_name{get_llvm_global_storage_symbol_name(runtime_module, global_index)};
    auto pointer{get_llvm_external_host_object_pointer(ir_builder,
                                                     storage_address,
                                                     llvm_value_type,
                                                     ::uwvm2::utils::container::u8string_view{symbol_name.data(), symbol_name.size()})};
    if(auto global{::llvm::dyn_cast_or_null<::llvm::GlobalVariable>(pointer)}; global != nullptr)
    {
        // State the native object's alignment on the declaration too; otherwise LLVM may infer a stronger ABI
        // alignment from the opaque integer type even when an individual load/store has a weaker explicit alignment.
        global->setAlignment(get_llvm_global_storage_alignment());
    }
    return pointer;
}

// C++ and LLVM agree on the four numeric scalar ABIs used below. v128 and references deliberately do not qualify:
// their internal LLVM opaque-integer representation is only a bit container and is not a promise that a C++ aggregate
// or native-vector value uses the same argument/return ABI on every supported target.
template <typename ValueType>
inline constexpr bool llvm_jit_local_imported_global_scalar_bridge_type =
    ::std::same_as<ValueType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32> ||
    ::std::same_as<ValueType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64> ||
    ::std::same_as<ValueType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32> ||
    ::std::same_as<ValueType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>;

enum class llvm_jit_local_imported_global_bridge_abi : unsigned char
{
    unsupported,
    scalar_value,
    byte_buffer
};

[[nodiscard]] inline constexpr llvm_jit_local_imported_global_bridge_abi
get_llvm_jit_local_imported_global_bridge_abi(runtime_operand_stack_value_type value_type) noexcept
{
    if(get_runtime_wasm_value_type_encoding(value_type) == 0x69u)
    { return llvm_jit_local_imported_global_bridge_abi::byte_buffer; }
    switch(value_type)
    {
        case runtime_operand_stack_value_type::i32:
        case runtime_operand_stack_value_type::i64:
        case runtime_operand_stack_value_type::f32:
        case runtime_operand_stack_value_type::f64:
            return llvm_jit_local_imported_global_bridge_abi::scalar_value;
        case runtime_operand_stack_value_type::v128:
        case runtime_operand_stack_value_type::funcref:
        case runtime_operand_stack_value_type::externref:
            return llvm_jit_local_imported_global_bridge_abi::byte_buffer;
        [[unlikely]] default:
            return llvm_jit_local_imported_global_bridge_abi::unsupported;
    }
}

// Host bridge used by generated code to read a local-imported numeric scalar. The constrained template prevents a
// future v128/reference call site from silently reintroducing an aggregate-by-value ABI dependency.
template <typename ValueType>
    requires llvm_jit_local_imported_global_scalar_bridge_type<ValueType>
[[nodiscard]] inline constexpr ValueType llvm_jit_local_imported_global_get_bridge(::std::uintptr_t local_imported_module_address,
                                                                                   ::std::size_t global_index) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    ValueType value{};
    ::uwvm2::runtime::lib::details::invoke_local_imported_provider_global_get(
        local_imported_module, global_index, reinterpret_cast<::std::byte*>(::std::addressof(value)));
    return value;
}

// Host bridge used by generated code to write a local-imported numeric scalar. Failure is fatal because validated JIT
// code should only request globals that the runtime resolved successfully at emission time.
template <typename ValueType>
    requires llvm_jit_local_imported_global_scalar_bridge_type<ValueType>
inline constexpr void
    llvm_jit_local_imported_global_set_bridge(::std::uintptr_t local_imported_module_address, ::std::size_t global_index, ValueType value) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_global_set(
           local_imported_module, global_index, reinterpret_cast<::std::byte const*>(::std::addressof(value)))) [[unlikely]]
    {
        ::fast_io::fast_terminate();
    }
}

// Non-scalar globals cross the host ABI only through addresses. The pointed-to object is an explicitly sized byte
// buffer in generated code; provider dispatch copies the native v128/reference payload into or out of that buffer.
// This remains correct even when LLVM's equal-width opaque integer and the C++ carrier have different calling ABIs.
inline constexpr void llvm_jit_local_imported_global_get_byte_buffer_bridge(::std::uintptr_t local_imported_module_address,
                                                                            ::std::size_t global_index,
                                                                            ::std::uintptr_t output_buffer_address) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    auto output_buffer{reinterpret_cast<::std::byte*>(output_buffer_address)};
    if(local_imported_module == nullptr || output_buffer == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    ::uwvm2::runtime::lib::details::invoke_local_imported_provider_global_get(local_imported_module, global_index, output_buffer);
}

inline constexpr void llvm_jit_local_imported_global_set_byte_buffer_bridge(::std::uintptr_t local_imported_module_address,
                                                                            ::std::size_t global_index,
                                                                            ::std::uintptr_t input_buffer_address) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    auto input_buffer{reinterpret_cast<::std::byte const*>(input_buffer_address)};
    if(local_imported_module == nullptr || input_buffer == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_global_set(local_imported_module, global_index, input_buffer)) [[unlikely]]
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
                    // linked function import: [safe current record] -> [next record may be null]
                    // ^^ curr: the current record was checked; the next walk iteration rejects a null target. No Wasm byte pointer moves.
                    curr = curr->target.imported_ptr;
                    // [safe current record] -> [next record may be null]
                    //                          ^^ curr is checked before its next dereference.
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

// Resolve a table element to a function reference and decide whether it belongs to the current module.  This helper is
// used when building JIT table views and by paths that can pre-resolve indirect targets.
[[nodiscard]] inline constexpr runtime_call_indirect_callee_resolution_t
    resolve_runtime_call_indirect_callee(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                         runtime_table_elem_storage_t const& elem,
                                         ::uwvm2::uwvm::runtime::storage::runtime_registry_type const* compiler_registry = nullptr) noexcept
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

            auto const callee_resolution{resolve_runtime_direct_callee(runtime_module, static_cast<validation_module_traits_t::wasm_u32>(func_index_uz), compiler_registry)};
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

    // Declared minimum, never a transient post-grow snapshot. Native memories
    // cannot shrink below their declared minimum; zero means no static proof.
    ::std::size_t declared_minimum_byte_length{};

    // Unknown/provider/imported memories conservatively retain observable loads.
    bool direct_memory_is_shared{true};

    // Stable base address for mmap-backed direct memory access.
    ::std::byte* stable_memory_begin{};

    // Mutable base slot for directly addressable single-thread allocator memories. Growth may replace its value.
    ::std::byte* const* memory_begin_value_p{};

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

[[nodiscard]] inline constexpr bool runtime_local_imported_page_size_is_representable(::std::uint_least64_t page_size_bytes) noexcept
{
    // The generated memory.size/grow paths use a shift and store the byte size in size_t. Reject a provider contract
    // that cannot be represented exactly instead of truncating it on 32-bit hosts or treating a non-power-of-two size
    // as 2^countr_zero(size).
    if(!::std::has_single_bit(page_size_bytes)) [[unlikely]] { return false; }
    if constexpr(::std::numeric_limits<::std::size_t>::digits < ::std::numeric_limits<::std::uint_least64_t>::digits)
    {
        if(page_size_bytes > static_cast<::std::uint_least64_t>((::std::numeric_limits<::std::size_t>::max)())) [[unlikely]] { return false; }
    }
    return true;
}

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
    result.memory_begin_value_p = ::std::addressof(memory.memory_begin);

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
    return ::uwvm2::runtime::compiler::shared::wasm_memory64::maximum_bytes_from_pages(
        static_cast<::std::uint_least64_t>(limits.max), 16u);
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
                    // linked function import: [safe current record] -> [next record may be null]
                    // ^^ curr: the current record was checked; the next walk iteration rejects a null target. No Wasm byte pointer moves.
                    curr = curr->target.imported_ptr;
                    // [safe current record] -> [next record may be null]
                    //                          ^^ curr is checked before its next dereference.
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
                    // Resolving a provider's compile-time page size is still an extensible virtual call. Keep it behind
                    // the same callback boundary used by generated accesses so compilation cannot inherit a live raw-
                    // bridge capability or leak provider FP-control changes into the active Wasm entry.
                    auto const page_size_bytes{
                        ::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_page_size_for_compilation(
                            result.local_imported_module_ptr, result.local_imported_memory_index)};
                    if(!runtime_local_imported_page_size_is_representable(page_size_bytes)) [[unlikely]] { return {}; }
                    result.local_imported_page_size_bytes = page_size_bytes;
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
    result.direct_memory_is_shared = ::uwvm2::runtime::wasm_threads::is_shared(local_memory_rec.memory);
    auto const page_log2{result.custom_page_size_log2};
    auto const min_pages{static_cast<::std::uint_least64_t>(local_memory_rec.effective_limits.min)};
    if(page_log2 < ::std::numeric_limits<::std::size_t>::digits &&
       min_pages <= (::std::numeric_limits<::std::size_t>::max() >> page_log2))
    { result.declared_minimum_byte_length = static_cast<::std::size_t>(min_pages) << page_log2; }
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
[[noreturn]] inline constexpr void llvm_jit_memory_bridge_trap(::std::size_t memory_idx,
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

// These C ABIs require extension attributes for i32 parameters/results that a
// bare LLVM i32 call does not express. Use full GPR carriers on those targets.
// Keep the original i32 ABI elsewhere: widening on x86 can pessimize constant
// materialization in narrow cmpxchg bridges. Guest i64 values always keep 64 bits.
#if defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || defined(_ARCH_PPC64) || defined(__s390x__) || \
    (defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen == 64) || \
    (defined(__mips__) && defined(_MIPS_SZPTR) && _MIPS_SZPTR == 64) || \
    (defined(__sparc__) && defined(__arch64__)) || (defined(__loongarch_grlen) && __loongarch_grlen == 64)
using llvm_jit_atomic_address_carrier_t = ::std::uint64_t;
#else
using llvm_jit_atomic_address_carrier_t = ::std::uint32_t;
#endif
// The emitter zero-extends validated u32 addresses/immediates into these carriers;
// they do not enable memory64. Wait timeouts retain their signed i64 interpretation.
template<typename ValueType>
using llvm_jit_atomic_value_carrier_t = ::std::conditional_t<(sizeof(ValueType) <= sizeof(::std::uint32_t)),
    llvm_jit_atomic_address_carrier_t, ::std::uint64_t>;

// Waiting bridges retain only the native owner identity across suspension. The
// VM entry lease owns that object; no pointer into a relocatable allocation is
// retained. Cancellation is a host trap, never a fourth WebAssembly return value.
template<unsigned Operation, bool ManagedShutdown = false>
[[nodiscard]] inline llvm_jit_atomic_address_carrier_t llvm_jit_atomic_wait_notify_bridge(::std::uintptr_t memory_address,
    llvm_jit_atomic_address_carrier_t offset, llvm_jit_atomic_address_carrier_t address,
    ::std::uint64_t expected, runtime_wasm_i64 timeout) noexcept(!ManagedShutdown)
{
    static_assert(Operation <= 2u);
    // [live native owner] embedded by translation; guest addresses cannot select
    // a native object, wait domain, cancellation token or callback.
    auto const memory{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const effective{static_cast<::std::uint64_t>(static_cast<::std::uint32_t>(address)) + offset};
    namespace waiting = ::uwvm2::runtime::wasm_threads;
    auto const result{[&]
    {
        if constexpr(Operation == 0u) { return waiting::memory_notify(*memory, effective, static_cast<::std::uint32_t>(expected)); }
        else { return waiting::memory_wait<Operation == 1u ? 4uz : 8uz>(*memory, waiting::is_shared(*memory), effective,
            static_cast<::std::uint64_t>(expected), timeout); }
    }()};
    using trap = ::uwvm2::runtime::lib::llvm_jit_trap_kind;
    trap failure{trap::runtime_invariant_failure};
    switch(result.status)
    {
        case waiting::wait_status::notified: case waiting::wait_status::not_equal: case waiting::wait_status::timed_out:
            return static_cast<llvm_jit_atomic_address_carrier_t>(Operation == 0u ? result.notified : static_cast<unsigned>(result.status));
        case waiting::wait_status::unaligned: failure=trap::unaligned_atomic; break;
        case waiting::wait_status::out_of_bounds:
            llvm_jit_memory_bridge_trap(memory->diagnostic_owner_memory_index, offset,
                llvm_jit_compute_wasm32_effective_offset(address, offset), result.memory_length, Operation == 2u ? 8uz : 4uz);
            ::fast_io::fast_terminate();
        case waiting::wait_status::not_shared: failure=trap::atomic_wait_non_shared; break;
        case waiting::wait_status::cancelled:
            if constexpr(ManagedShutdown) { ::uwvm2::runtime::lib::details::llvm_jit_debug_cancelled_wait_abi_bridge(); }
            failure=trap::atomic_wait_cancelled; break;
        case waiting::wait_status::unavailable: failure=trap::atomic_wait_unavailable; break;
        case waiting::wait_status::too_many_waiters: failure=trap::atomic_wait_limit; break;
    }
    // All registry/pinning scopes ended before entering either stack reporter.
    ::uwvm2::runtime::lib::llvm_jit_runtime_trap(failure, 0u, 0u);
    ::fast_io::fast_terminate();
}

// Memory64 waiting uses exact i64 address/immediate arguments on every host
// ABI. Only the i32 return carrier follows the platform extension convention.
template<unsigned Operation, bool ManagedShutdown = false>
[[nodiscard]] inline llvm_jit_atomic_address_carrier_t llvm_jit_memory64_wait_notify_bridge(::std::uintptr_t memory_address,
    ::std::uint64_t offset, ::std::uint64_t address,
    ::std::uint64_t expected, runtime_wasm_i64 timeout) noexcept(!ManagedShutdown)
{
    static_assert(Operation <= 2u);
    // [live native owner] embedded by translation; guest addresses cannot select
    // a native object, wait domain, cancellation token or callback.
    auto const memory{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const effective{address + offset};
    namespace waiting = ::uwvm2::runtime::wasm_threads;
    auto const result{waiting::memory_wait_notify64<Operation>(*memory, offset, address, expected, timeout)};
    using trap = ::uwvm2::runtime::lib::llvm_jit_trap_kind;
    trap failure{trap::runtime_invariant_failure};
    switch(result.status)
    {
        case waiting::wait_status::notified: case waiting::wait_status::not_equal: case waiting::wait_status::timed_out:
            return static_cast<llvm_jit_atomic_address_carrier_t>(Operation == 0u ? result.notified : static_cast<unsigned>(result.status));
        case waiting::wait_status::unaligned: failure=trap::unaligned_atomic; break;
        case waiting::wait_status::out_of_bounds:
            ::uwvm2::runtime::lib::llvm_jit_memory_out_of_bounds_trap(memory->diagnostic_owner_memory_index, offset,
                effective, effective < address ? 1u : 0u, result.memory_length, Operation == 2u ? 8uz : 4uz, 0u, 0u);
            ::fast_io::fast_terminate();
        case waiting::wait_status::not_shared: failure=trap::atomic_wait_non_shared; break;
        case waiting::wait_status::cancelled:
            if constexpr(ManagedShutdown) { ::uwvm2::runtime::lib::details::llvm_jit_debug_cancelled_wait_abi_bridge(); }
            failure=trap::atomic_wait_cancelled; break;
        case waiting::wait_status::unavailable: failure=trap::atomic_wait_unavailable; break;
        case waiting::wait_status::too_many_waiters: failure=trap::atomic_wait_limit; break;
    }
    // All registry/pinning scopes ended before entering either stack reporter.
    ::uwvm2::runtime::lib::llvm_jit_runtime_trap(failure, 0u, 0u);
    ::fast_io::fast_terminate();
}

// These four VM-owned blocking leaves preserve the genuine Wasm activation.
// A foreign-host wrapper would hide its live frame while the linked wait parks.
// Notify and arbitrary host/provider bridges retain their original isolation.
template<> inline constexpr bool llvm_jit_debug_control_bridge<llvm_jit_atomic_wait_notify_bridge<1u>>{true};
template<> inline constexpr bool llvm_jit_debug_control_bridge<llvm_jit_atomic_wait_notify_bridge<2u>>{true};
template<> inline constexpr bool llvm_jit_debug_control_bridge<llvm_jit_memory64_wait_notify_bridge<1u>>{true};
template<> inline constexpr bool llvm_jit_debug_control_bridge<llvm_jit_memory64_wait_notify_bridge<2u>>{true};
template<> inline constexpr bool llvm_jit_debug_control_bridge<llvm_jit_atomic_wait_notify_bridge<1u, true>>{true};
template<> inline constexpr bool llvm_jit_debug_control_bridge<llvm_jit_atomic_wait_notify_bridge<2u, true>>{true};
template<> inline constexpr bool llvm_jit_debug_control_bridge<llvm_jit_memory64_wait_notify_bridge<1u, true>>{true};
template<> inline constexpr bool llvm_jit_debug_control_bridge<llvm_jit_memory64_wait_notify_bridge<2u, true>>{true};

// Function-template values can have identical __PRETTY_FUNCTION__ spellings
// on Clang despite different non-type arguments. Include the guest operation
// explicitly so notify/wait32/wait64 never share a cached bridge relocation.
// Callers pass a VM-owned object address, never a guest-selected native pointer.
template<unsigned Operation>
[[nodiscard]] inline ::llvm::Value* emit_llvm_jit_memory64_wait_notify_call(::llvm::IRBuilder<>& b,
    ::llvm::Value* owner, ::llvm::Value* offset, ::llvm::Value* address, ::llvm::Value* expected, ::llvm::Value* timeout) noexcept
{
    static_assert(Operation <= 2u);
    auto const intptr{b.getIntNTy(sizeof(::std::uintptr_t) * CHAR_BIT)};
    if(owner == nullptr || owner->getType() != intptr || offset == nullptr || address == nullptr || expected == nullptr || timeout == nullptr ||
       !offset->getType()->isIntegerTy(64u) || !address->getType()->isIntegerTy(64u) ||
       !expected->getType()->isIntegerTy(64u) || !timeout->getType()->isIntegerTy(64u)) { return nullptr; }
    auto const carrier{b.getIntNTy(sizeof(llvm_jit_atomic_address_carrier_t) * CHAR_BIT)};
    auto const type{::llvm::FunctionType::get(carrier, {intptr, b.getInt64Ty(), b.getInt64Ty(), b.getInt64Ty(), b.getInt64Ty()}, false)};
    constexpr ::uwvm2::utils::container::u8string_view discriminator{Operation == 0u ? u8"memory64.atomic.notify" :
        Operation == 1u ? u8"memory64.atomic.wait32" : u8"memory64.atomic.wait64"};
    auto const bridge{get_llvm_runtime_bridge_function_symbol_value<llvm_jit_memory64_wait_notify_bridge<Operation>>(b, type, discriminator)};
    if(bridge == nullptr) { return nullptr; }
    auto const call{b.CreateCall(type, bridge, {owner, offset, address, expected, timeout})};
    apply_llvm_jit_host_calling_conv(call);
    return b.CreateZExtOrTrunc(call, b.getInt32Ty());
}

// Address carriers are independent of value carriers. An i32 atomic value
// retains its qualified native ABI even when the selected memory uses i64.
template<bool Address64>
using llvm_jit_typed_atomic_address_t = ::std::conditional_t<Address64, ::std::uint64_t, llvm_jit_atomic_address_carrier_t>;

// Atomic fallback pins a moving allocation for the actual access. It never
// implements an atomic operation through a non-atomic provider read callback.
template <typename ResultType, ::std::size_t Bytes, bool Address64 = false>
[[nodiscard]] inline llvm_jit_atomic_value_carrier_t<ResultType> llvm_jit_atomic_load_bridge(::std::uintptr_t memory_address,
    llvm_jit_typed_atomic_address_t<Address64> static_offset, llvm_jit_typed_atomic_address_t<Address64> address) noexcept
{
    auto const effective{[&]() constexpr noexcept -> llvm_jit_wasm32_effective_offset_t
    {
        if constexpr(Address64)
        { auto const low{address + static_offset}; return {low, low < address}; }
        else { return llvm_jit_compute_wasm32_effective_offset(address, static_offset); }
    }()};
    if((effective.offset & (Bytes - 1uz)) != 0u) [[unlikely]]
    {
        ::uwvm2::runtime::lib::llvm_jit_runtime_trap(::uwvm2::runtime::lib::llvm_jit_trap_kind::unaligned_atomic, 0u, 0u);
        ::fast_io::fast_terminate();
    }
    // [live native memory object] address is embedded by the validated translator,
    // never supplied as a guest linear address; the host-entry lease keeps it alive.
    auto const memory{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    llvm_jit_atomic_value_carrier_t<ResultType> result{};
    ::std::size_t memory_length{};
    if(!llvm_jit_try_checked_memory_access(*memory, effective, Bytes, memory_length,
        [&](::std::byte* begin, ::std::size_t offset) noexcept
        {
            // [live pinned memory ... offset: Bytes ...]
            //                         ^^ bounded by checked access; naturally aligned.
            auto const pointer{begin + offset};
            result = static_cast<llvm_jit_atomic_value_carrier_t<ResultType>>(::uwvm2::runtime::compiler::shared::wasm_threads::atomic_load_le<Bytes>(pointer));
        })) [[unlikely]]
    {
        if constexpr(Address64)
        {
            ::uwvm2::runtime::lib::llvm_jit_memory_out_of_bounds_trap(memory->diagnostic_owner_memory_index,
                static_offset, effective.offset, effective.offset_65_bit ? 1u : 0u, memory_length, Bytes, 0u, 0u);
            ::fast_io::fast_terminate();
        }
        else { llvm_jit_memory_bridge_trap(memory->diagnostic_owner_memory_index, static_offset, effective, memory_length, Bytes); }
    }
    return result;
}

// Store counterpart retains the same allocation pin through the atomic write.
template <typename ValueType, ::std::size_t Bytes, bool Address64 = false>
inline void llvm_jit_atomic_store_bridge(::std::uintptr_t memory_address,
    llvm_jit_typed_atomic_address_t<Address64> static_offset, llvm_jit_typed_atomic_address_t<Address64> address, llvm_jit_atomic_value_carrier_t<ValueType> value) noexcept
{
    auto const effective{[&]() constexpr noexcept -> llvm_jit_wasm32_effective_offset_t
    {
        if constexpr(Address64)
        { auto const low{address + static_offset}; return {low, low < address}; }
        else { return llvm_jit_compute_wasm32_effective_offset(address, static_offset); }
    }()};
    if((effective.offset & (Bytes - 1uz)) != 0u) [[unlikely]]
    {
        ::uwvm2::runtime::lib::llvm_jit_runtime_trap(::uwvm2::runtime::lib::llvm_jit_trap_kind::unaligned_atomic, 0u, 0u);
        ::fast_io::fast_terminate();
    }
    // [live native memory object] embedded by the validated translator; not a
    // guest address. The runtime entry lease retains the instance until return.
    auto const memory{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    ::std::size_t memory_length{};
    if(!llvm_jit_try_checked_memory_access(*memory, effective, Bytes, memory_length,
        [&](::std::byte* begin, ::std::size_t offset) noexcept
        {
            // [live pinned memory ... offset: Bytes ...]
            //                         ^^ complete range and natural alignment proved.
            auto const pointer{begin + offset};
            ::uwvm2::runtime::compiler::shared::wasm_threads::atomic_store_le<Bytes>(pointer, static_cast<::std::uint64_t>(value));
        })) [[unlikely]]
    {
        if constexpr(Address64)
        {
            ::uwvm2::runtime::lib::llvm_jit_memory_out_of_bounds_trap(memory->diagnostic_owner_memory_index,
                static_offset, effective.offset, effective.offset_65_bit ? 1u : 0u, memory_length, Bytes, 0u, 0u);
            ::fast_io::fast_terminate();
        }
        else { llvm_jit_memory_bridge_trap(memory->diagnostic_owner_memory_index, static_offset, effective, memory_length, Bytes); }
    }
}

template <::uwvm2::runtime::compiler::shared::wasm_threads::atomic_rmw_operation Operation, typename ValueType, ::std::size_t Bytes, bool Address64 = false>
[[nodiscard]] inline llvm_jit_atomic_value_carrier_t<ValueType> llvm_jit_atomic_rmw_bridge(::std::uintptr_t memory_address,
    llvm_jit_typed_atomic_address_t<Address64> static_offset, llvm_jit_typed_atomic_address_t<Address64> address, llvm_jit_atomic_value_carrier_t<ValueType> value, llvm_jit_atomic_value_carrier_t<ValueType> expected) noexcept
{
    auto const effective{[&]() constexpr noexcept -> llvm_jit_wasm32_effective_offset_t
    {
        if constexpr(Address64)
        { auto const low{address + static_offset}; return {low, low < address}; }
        else { return llvm_jit_compute_wasm32_effective_offset(address, static_offset); }
    }()};
    if((effective.offset & (Bytes - 1uz)) != 0u) [[unlikely]]
    {
        ::uwvm2::runtime::lib::llvm_jit_runtime_trap(::uwvm2::runtime::lib::llvm_jit_trap_kind::unaligned_atomic, 0u, 0u);
        ::fast_io::fast_terminate();
    }
    // [live native memory object] host-owned address embedded during validation;
    // the execution admission keeps it alive through this entire bridge call.
    auto const memory{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    llvm_jit_atomic_value_carrier_t<ValueType> result{};
    ::std::size_t memory_length{};
    if(!llvm_jit_try_checked_memory_access(*memory, effective, Bytes, memory_length,
        [&](::std::byte* begin, ::std::size_t offset) noexcept
        {
            // [live pinned allocation ... offset: Bytes ...]
            //                             ^^ checked full range and natural alignment.
            auto const pointer{begin + offset};
            result = static_cast<llvm_jit_atomic_value_carrier_t<ValueType>>(::uwvm2::runtime::compiler::shared::wasm_threads::atomic_rmw_le<Operation, Bytes>(
                pointer, static_cast<::std::uint64_t>(value), static_cast<::std::uint64_t>(expected)));
        })) [[unlikely]]
    {
        if constexpr(Address64)
        {
            ::uwvm2::runtime::lib::llvm_jit_memory_out_of_bounds_trap(memory->diagnostic_owner_memory_index,
                static_offset, effective.offset, effective.offset_65_bit ? 1u : 0u, memory_length, Bytes, 0u, 0u);
            ::fast_io::fast_terminate();
        }
        else { llvm_jit_memory_bridge_trap(memory->diagnostic_owner_memory_index, static_offset, effective, memory_length, Bytes); }
    }
    return result;
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
    if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_read(
           local_imported_module, memory_index, effective_offset.offset, load_buffer, LoadBytes)) [[unlikely]]
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

    if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_write(
           local_imported_module, memory_index, effective_offset.offset, store_buffer, StoreBytes)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
    }
}

// Only memory backends that require a provider callback or a moving-allocation pin use SIMD buffers. Arithmetic and
// directly addressable memory use vector IR below; no per-op host ABI boundary remains on the native fast path.
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


// Native fallback for a memory64 access. The generic range checker stores
// its low offset in u64 on every host; construct a genuine u65 sum here instead
// of applying the memory32 computation's u33 overflow rule.
template<typename Fn>
inline void llvm_jit_with_checked_memory64_access(::std::uintptr_t owner, ::std::uint64_t offset,
    ::std::uint64_t address, ::std::size_t width, Fn&& fn) noexcept
{
    // [live native memory owner] retained by the current VM execution lease.
    // ^^ memory: translator-owned relocation, never an address chosen by Wasm.
    auto const memory{reinterpret_cast<runtime_native_memory_t*>(owner)};
    if(memory == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const low{address + offset};
    llvm_jit_wasm32_effective_offset_t const effective{low, low < address};
    ::std::size_t length{};
    // The shared checker pins only moving allocations; mmap snapshots take no
    // operation lock. No pointer arithmetic or narrowing occurs before its full
    // u64-versus-native allocation proof, including on a 32-bit host.
    if(!llvm_jit_try_checked_memory_access(*memory, effective, width, length, ::std::forward<Fn>(fn))) [[unlikely]]
    {
        ::uwvm2::runtime::lib::llvm_jit_memory_out_of_bounds_trap(memory->diagnostic_owner_memory_index,
            offset, low, effective.offset_65_bit ? 1u : 0u, length, width, 0u, 0u);
        ::fast_io::fast_terminate();
    }
}

// Use one unsigned i64 carrier for scalar bit patterns on every native ABI.
// This keeps i32 ABI extension rules and floating-point NaN canonicalization out
// of fallback calls; the LLVM caller reconstructs the exact Wasm result type.
template<unsigned ResultBits, ::std::size_t Bytes, bool Signed>
[[nodiscard]] inline ::std::uint64_t llvm_jit_memory64_scalar_load_bridge(
    ::std::uintptr_t owner, ::std::uint64_t offset, ::std::uint64_t address) noexcept
{
    static_assert(ResultBits == 32u || ResultBits == 64u);
    static_assert(Bytes == 1uz || Bytes == 2uz || Bytes == 4uz || Bytes == 8uz);
    static_assert(Bytes * 8uz <= ResultBits);
    ::std::uint64_t result{};
    llvm_jit_with_checked_memory64_access(owner, offset, address, Bytes,
        [&](::std::byte* begin, ::std::size_t effective) noexcept
    {
        // [native allocation ... complete Bytes-byte source ...] end
        // [safe                                                ]
        //                        ^^ pointer; the checker proved the entire u65
        // address and native span before this host pointer addition.
        auto const pointer{begin + effective};
        if constexpr(Bytes == 1uz) { result = llvm_jit_load_little_endian_integer<::std::uint8_t>(pointer); }
        else if constexpr(Bytes == 2uz) { result = llvm_jit_load_little_endian_integer<::std::uint16_t>(pointer); }
        else if constexpr(Bytes == 4uz) { result = llvm_jit_load_little_endian_integer<::std::uint32_t>(pointer); }
        else { result = llvm_jit_load_little_endian_integer<::std::uint64_t>(pointer); }
    });
    if constexpr(Signed && Bytes * 8uz < ResultBits)
    {
        constexpr auto sign{::std::uint64_t{1u} << (Bytes * 8uz - 1uz)};
        result = (result ^ sign) - sign; // Defined unsigned extension, including the i64 sign bit.
    }
    if constexpr(ResultBits == 32u) { result = static_cast<::std::uint32_t>(result); }
    return result;
}

template<::std::size_t Bytes>
inline void llvm_jit_memory64_scalar_store_bridge(::std::uintptr_t owner,
    ::std::uint64_t offset, ::std::uint64_t address, ::std::uint64_t bits) noexcept
{
    static_assert(Bytes == 1uz || Bytes == 2uz || Bytes == 4uz || Bytes == 8uz);
    llvm_jit_with_checked_memory64_access(owner, offset, address, Bytes,
        [&](::std::byte* begin, ::std::size_t effective) noexcept
    {
        // [native allocation ... complete Bytes-byte destination ...] end
        // [safe                                                     ]
        //                        ^^ pointer; no partial write occurs before
        // checking the full span or while a moving allocation is unpinned.
        auto const pointer{begin + effective};
        if constexpr(Bytes == 1uz) { llvm_jit_store_little_endian_integer(pointer, static_cast<::std::uint8_t>(bits)); }
        else if constexpr(Bytes == 2uz) { llvm_jit_store_little_endian_integer(pointer, static_cast<::std::uint16_t>(bits)); }
        else if constexpr(Bytes == 4uz) { llvm_jit_store_little_endian_integer(pointer, static_cast<::std::uint32_t>(bits)); }
        else { llvm_jit_store_little_endian_integer(pointer, bits); }
    });
}

template<unsigned ValueBits, ::std::size_t Bytes, bool Signed, bool Store>
[[nodiscard]] inline ::llvm::Value* emit_llvm_jit_memory64_scalar_call(::llvm::IRBuilder<>& b,
    ::llvm::Value* owner, ::llvm::Value* offset, ::llvm::Value* address, ::llvm::Type* value_type,
    ::llvm::Value* value = nullptr) noexcept
{
    static_assert(ValueBits == 32u || ValueBits == 64u);
    static_assert(Bytes == 1uz || Bytes == 2uz || Bytes == 4uz || Bytes == 8uz);
    static_assert(Bytes * 8uz <= ValueBits);
    auto const intptr{b.getIntNTy(sizeof(::std::uintptr_t) * CHAR_BIT)};
    if(owner == nullptr || owner->getType() != intptr || offset == nullptr || !offset->getType()->isIntegerTy(64u) ||
       address == nullptr || !address->getType()->isIntegerTy(64u) || value_type == nullptr ||
       !(value_type->isIntegerTy(ValueBits) || (ValueBits == 32u ? value_type->isFloatTy() : value_type->isDoubleTy())))
    { return nullptr; }
    if constexpr(Store)
    {
        if(value == nullptr || value->getType() != value_type) { return nullptr; }
        auto const type{::llvm::FunctionType::get(b.getVoidTy(), {intptr, b.getInt64Ty(), b.getInt64Ty(), b.getInt64Ty()}, false)};
        auto const discriminator{::fast_io::u8concat_fast_io(u8"memory64.scalar.store.", Bytes)};
        auto const bridge{get_llvm_runtime_bridge_function_symbol_value<llvm_jit_memory64_scalar_store_bridge<Bytes>>(
            b, type, ::uwvm2::utils::container::u8string_view{discriminator.data(), discriminator.size()})};
        if(bridge == nullptr) { return nullptr; }
        // Bitcasts do not perform floating-point arithmetic, including for signalling NaNs.
        auto const bits{b.CreateZExtOrTrunc(b.CreateBitCast(value, b.getIntNTy(ValueBits)), b.getInt64Ty())};
        auto const call{b.CreateCall(type, bridge, {owner, offset, address, bits})};
        apply_llvm_jit_host_calling_conv(call);
        return call; // Function-owned IR; no guest pointer is exposed to the native ABI.
    }
    else
    {
        auto const type{::llvm::FunctionType::get(b.getInt64Ty(), {intptr, b.getInt64Ty(), b.getInt64Ty()}, false)};
        auto const discriminator{::fast_io::u8concat_fast_io(u8"memory64.scalar.load.", ValueBits, u8".", Bytes, u8".", Signed)};
        auto const bridge{get_llvm_runtime_bridge_function_symbol_value<llvm_jit_memory64_scalar_load_bridge<ValueBits, Bytes, Signed>>(
            b, type, ::uwvm2::utils::container::u8string_view{discriminator.data(), discriminator.size()})};
        if(bridge == nullptr) { return nullptr; }
        auto const call{b.CreateCall(type, bridge, {owner, offset, address})};
        apply_llvm_jit_host_calling_conv(call);
        return b.CreateBitCast(b.CreateZExtOrTrunc(call, b.getIntNTy(ValueBits)), value_type);
    }
}

template<llvm_jit_simd_code Op>
inline void llvm_jit_memory64_simd_load_bridge(::std::uintptr_t owner, ::std::uint64_t offset,
    ::std::uint64_t address, ::std::uintptr_t old_address, llvm_jit_atomic_address_carrier_t lane,
    ::std::uintptr_t result_address) noexcept
{
    runtime_wasm_v128 old{};
    if constexpr(Op == llvm_jit_simd_code::v128_load8_lane || Op == llvm_jit_simd_code::v128_load16_lane ||
                 Op == llvm_jit_simd_code::v128_load32_lane || Op == llvm_jit_simd_code::v128_load64_lane)
    { old = llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(old_address); }
    constexpr auto width{llvm_jit_simd_details::simd_memory_access_size<Op>()};
    runtime_wasm_v128 result{};
    llvm_jit_with_checked_memory64_access(owner, offset, address, width, [&](::std::byte* begin, ::std::size_t effective) noexcept
    {
        // [native allocation ... complete width-byte span ...] end
        // [safe                                              ]
        //                        ^^ pointer; effective was narrowed only after
        // checking both the complete u65 sum and the entire native access width.
        auto const pointer{begin + effective};
        result = llvm_jit_simd_details::eval_memory_load<Op>(pointer, old, static_cast<llvm_jit_simd_details::u8>(lane));
    });
    // Result/old buffers are translator-owned sixteen-byte temporaries, whose
    // lifetimes extend through this call; they are never guest linear pointers.
    llvm_jit_simd_write_bridge_value(result_address, result);
}

template<llvm_jit_simd_code Op>
inline void llvm_jit_memory64_simd_store_bridge(::std::uintptr_t owner, ::std::uint64_t offset,
    ::std::uint64_t address, ::std::uintptr_t value_address, llvm_jit_atomic_address_carrier_t lane) noexcept
{
    auto const value{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(value_address)};
    constexpr auto width{llvm_jit_simd_details::simd_memory_access_size<Op>()};
    llvm_jit_with_checked_memory64_access(owner, offset, address, width, [&](::std::byte* begin, ::std::size_t effective) noexcept
    {
        // [native allocation ... complete width-byte destination ...] end
        // [safe                                                     ]
        //                        ^^ pointer; all bounds checks precede any mutation.
        auto const pointer{begin + effective};
        llvm_jit_simd_details::eval_memory_store<Op>(pointer, value, static_cast<llvm_jit_simd_details::u8>(lane));
    });
}

template<llvm_jit_simd_code Op>
[[nodiscard]] inline ::llvm::CallInst* emit_llvm_jit_memory64_simd_call(::llvm::IRBuilder<>& b,
    ::llvm::Value* owner, ::llvm::Value* offset, ::llvm::Value* address, ::llvm::Value* vector_address,
    unsigned lane, ::llvm::Value* result_address = nullptr) noexcept
{
    constexpr bool store{Op == llvm_jit_simd_code::v128_store || Op == llvm_jit_simd_code::v128_store8_lane ||
        Op == llvm_jit_simd_code::v128_store16_lane || Op == llvm_jit_simd_code::v128_store32_lane || Op == llvm_jit_simd_code::v128_store64_lane};
    constexpr auto width{llvm_jit_simd_details::simd_memory_access_size<Op>()};
    auto const intptr{b.getIntNTy(sizeof(::std::uintptr_t) * CHAR_BIT)};
    auto const carrier{b.getIntNTy(sizeof(llvm_jit_atomic_address_carrier_t) * CHAR_BIT)};
    if(owner == nullptr || owner->getType() != intptr || offset == nullptr || !offset->getType()->isIntegerTy(64u) ||
       address == nullptr || !address->getType()->isIntegerTy(64u) || vector_address == nullptr || vector_address->getType() != intptr ||
       lane >= 16uz / width) { return nullptr; }
    // Include the opcode explicitly: same-signature template instantiations can
    // have identical Clang pretty-function spellings, which cannot name distinct
    // bridge relocations in the JIT cache.
    auto const discriminator{::fast_io::u8concat_fast_io(u8"memory64.simd.", static_cast<unsigned>(Op))};
    auto const tag{::uwvm2::utils::container::u8string_view{discriminator.data(), discriminator.size()}};
    ::llvm::CallInst* call{};
    if constexpr(store)
    {
        auto const type{::llvm::FunctionType::get(b.getVoidTy(), {intptr, b.getInt64Ty(), b.getInt64Ty(), intptr, carrier}, false)};
        auto const bridge{get_llvm_runtime_bridge_function_symbol_value<llvm_jit_memory64_simd_store_bridge<Op>>(b, type, tag)};
        if(bridge == nullptr) { return nullptr; }
        call = b.CreateCall(type, bridge, {owner, offset, address, vector_address, ::llvm::ConstantInt::get(carrier, lane)});
        // call borrows the newly created function-owned IR node; no guest pointer is formed.
    }
    else
    {
        if(result_address == nullptr || result_address->getType() != intptr) { return nullptr; }
        auto const type{::llvm::FunctionType::get(b.getVoidTy(), {intptr, b.getInt64Ty(), b.getInt64Ty(), intptr, carrier, intptr}, false)};
        auto const bridge{get_llvm_runtime_bridge_function_symbol_value<llvm_jit_memory64_simd_load_bridge<Op>>(b, type, tag)};
        if(bridge == nullptr) { return nullptr; }
        call = b.CreateCall(type, bridge, {owner, offset, address, vector_address, ::llvm::ConstantInt::get(carrier, lane), result_address});
        // call borrows the new call node; both native buffer identities came from translation.
    }
    apply_llvm_jit_host_calling_conv(call);
    return call;
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
    if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_read(
           local_imported_module, memory_index, effective_offset.offset, bytes, access_size)) [[unlikely]]
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
    if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_write(
           local_imported_module, memory_index, effective_offset.offset, bytes, access_size)) [[unlikely]]
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

    ::uwvm2::runtime::lib::details::local_imported_provider_memory_snapshot_t snapshot{};
    if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_access_snapshot(local_imported_module, memory_index, snapshot)) [[unlikely]]
    {
        return false;
    }

    auto const page_size_bytes_u64{
        ::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_page_size(local_imported_module, memory_index)};
    if(!runtime_local_imported_page_size_is_representable(page_size_bytes_u64)) [[unlikely]] { return false; }

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

// Core 3 bulk-memory bridge ABIs preserve the declared address widths. In a
// mixed copy the length is i32 even when one of the addresses is i64.
template<bool Address64>
using llvm_jit_bulk_address_t = ::std::conditional_t<Address64, runtime_wasm_i64, runtime_wasm_i32>;

template<typename Integer>
[[nodiscard]] inline constexpr ::std::uint_least64_t llvm_jit_bulk_unsigned(Integer value) noexcept
{ return static_cast<::std::make_unsigned_t<Integer>>(value); }

[[nodiscard]] inline constexpr ::std::size_t llvm_jit_bulk_diagnostic_length(::std::uint_least64_t length) noexcept
{
    constexpr auto maximum{::std::numeric_limits<::std::size_t>::max()};
    return length > maximum ? maximum : static_cast<::std::size_t>(length);
}

template<bool Destination64, bool Source64>
inline void llvm_jit_memory64_copy_bridge(::std::uintptr_t destination_address, ::std::uintptr_t source_address,
    llvm_jit_bulk_address_t<Destination64> destination_value, llvm_jit_bulk_address_t<Source64> source_value,
    llvm_jit_bulk_address_t<Destination64 && Source64> length_value) noexcept
{
    namespace wide = ::uwvm2::runtime::compiler::shared::wasm_memory64;
    // Native memory objects come from validated relocation records. Wasm
    // operands below are byte offsets, never interpreted as native object pointers.
    // [live runtime memory object] or null, checked before a snapshot is acquired.
    auto const destination_memory{reinterpret_cast<runtime_native_memory_t*>(destination_address)};
    auto const source_memory{reinterpret_cast<runtime_native_memory_t*>(source_address)};
    if(destination_memory == nullptr || source_memory == nullptr) [[unlikely]] { llvm_jit_memory_bridge_trap(); }
    auto const destination{llvm_jit_bulk_unsigned(destination_value)};
    auto const source{llvm_jit_bulk_unsigned(source_value)};
    auto const length{llvm_jit_bulk_unsigned(length_value)};
    auto const transfer{[&](::std::byte* destination_begin, ::std::size_t destination_size,
                           ::std::byte* source_begin, ::std::size_t source_size) noexcept
    {
        auto const status{wide::copy(destination_begin, destination_size, source_begin, source_size,
                                     destination, source, length)};
        if(status != wide::bulk_error::none) [[unlikely]]
        {
            bool const source_failed{status == wide::bulk_error::source};
            llvm_jit_memory_bridge_trap(0uz, 0u, {source_failed ? source : destination, false},
                source_failed ? source_size : destination_size, llvm_jit_bulk_diagnostic_length(length));
        }
        return true;
    }};
    if(destination_memory == source_memory)
    {
        static_cast<void>(llvm_jit_with_bulk_memory_snapshot(*destination_memory,
            [&](::std::byte* begin, ::std::size_t size) noexcept { return transfer(begin, size, begin, size); }));
        return;
    }
    bool const destination_first{destination_address < source_address};
    auto& first{destination_first ? *destination_memory : *source_memory};
    auto& second{destination_first ? *source_memory : *destination_memory};
    static_cast<void>(llvm_jit_with_bulk_memory_snapshot(first, [&](::std::byte* first_begin, ::std::size_t first_size) noexcept
    {
        return llvm_jit_with_bulk_memory_snapshot(second, [&](::std::byte* second_begin, ::std::size_t second_size) noexcept
        {
            // [two pinned/stable allocations]; the transfer forms byte pointers
            // only after checking both complete full-width ranges.
            return destination_first ? transfer(first_begin, first_size, second_begin, second_size) :
                                       transfer(second_begin, second_size, first_begin, first_size);
        });
    }));
}

template<bool Address64>
inline void llvm_jit_memory64_fill_bridge(::std::uintptr_t memory_address, llvm_jit_bulk_address_t<Address64> destination_value,
    runtime_wasm_i32 value, llvm_jit_bulk_address_t<Address64> length_value) noexcept
{
    namespace wide = ::uwvm2::runtime::compiler::shared::wasm_memory64;
    // [live runtime memory object] or null; native relocation identity only.
    auto const memory{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory == nullptr) [[unlikely]] { llvm_jit_memory_bridge_trap(); }
    auto const destination{llvm_jit_bulk_unsigned(destination_value)};
    auto const length{llvm_jit_bulk_unsigned(length_value)};
    static_cast<void>(llvm_jit_with_bulk_memory_snapshot(*memory, [&](::std::byte* begin, ::std::size_t size) noexcept
    {
        if(wide::fill(begin, size, destination, static_cast<runtime_wasm_u32>(value), length) != wide::bulk_error::none) [[unlikely]]
        { llvm_jit_memory_bridge_trap(0uz, 0u, {destination, false}, size, llvm_jit_bulk_diagnostic_length(length)); }
        return true;
    }));
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


// A distinct source/destination pair keeps both allocations stable in total object-address order.
// The alias case reuses the one-lock bridge. Both ranges are checked before a byte is written.
inline constexpr void llvm_jit_cross_native_memory_copy_bridge(::std::uintptr_t destination_address,
    ::std::uintptr_t source_address, runtime_wasm_i32 dst_i32, runtime_wasm_i32 src_i32, runtime_wasm_i32 len_i32) noexcept
{
    if(destination_address == source_address)
    {
        llvm_jit_memory_copy_bridge(destination_address, dst_i32, src_i32, len_i32);
        return;
    }
    // Host object addresses are relocation records emitted from the validated module graph, never Wasm addresses.
    auto const destination{reinterpret_cast<runtime_native_memory_t*>(destination_address)};
    auto const source{reinterpret_cast<runtime_native_memory_t*>(source_address)};
    if(destination == nullptr || source == nullptr) { llvm_jit_memory_bridge_trap(); }
    auto const dst{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(dst_i32))};
    auto const src{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(src_i32))};
    auto const len{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(len_i32))};
    bool const destination_first{destination_address < source_address};
    auto& first{destination_first ? *destination : *source};
    auto& second{destination_first ? *source : *destination};
    static_cast<void>(llvm_jit_with_bulk_memory_snapshot(first, [&](::std::byte* first_begin, ::std::size_t first_length) constexpr noexcept
    {
        return llvm_jit_with_bulk_memory_snapshot(second, [&](::std::byte* second_begin, ::std::size_t second_length) constexpr noexcept
        {
            // Both snapshot pointers are protected by live access guards for the entire transfer.
            auto const dst_begin{destination_first ? first_begin : second_begin};
            auto const src_begin{destination_first ? second_begin : first_begin};
            auto const dst_length{destination_first ? first_length : second_length};
            auto const src_length{destination_first ? second_length : first_length};
            if(llvm_jit_bulk_memory_range_oob(src, len, src_length))
                { llvm_jit_memory_bridge_trap(0uz, 0u, {.offset = src, .offset_65_bit = false}, src_length, len); }
            if(llvm_jit_bulk_memory_range_oob(dst, len, dst_length))
                { llvm_jit_memory_bridge_trap(0uz, 0u, {.offset = dst, .offset_65_bit = false}, dst_length, len); }
            if(len != 0uz)
            {
                if(dst_begin == nullptr || src_begin == nullptr) { llvm_jit_memory_bridge_trap(); }
                // [dst, dst+len) and [src, src+len) are inside their respective guarded snapshots.
                ::std::memmove(dst_begin + dst, src_begin + src, len);
            }
            return true;
        });
    }));
}

// Host providers can relocate their allocations during grow. Retain no provider snapshot pointer:
// check full ranges first, then transfer through provider-owned operations using bounded staging.
inline constexpr void llvm_jit_wide_cross_provider_memory_copy_bridge(::std::uintptr_t runtime_module_address,
    ::std::uintptr_t destination_index, ::std::uintptr_t source_index,
    ::std::uint64_t dst, ::std::uint64_t src, ::std::uint64_t len) noexcept
{
    // Trusted JIT relocation: the owning runtime module outlives this compiled function.
    auto const module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    if(module == nullptr) { llvm_jit_memory_bridge_trap(); }
    auto const destination{resolve_runtime_memory_access_info(*module, destination_index)};
    auto const source{resolve_runtime_memory_access_info(*module, source_index)};
    auto const range_valid{[&](runtime_memory_access_info_t const& info, ::std::uint64_t offset) constexpr noexcept
    {
        if(info.memory_p != nullptr)
            return llvm_jit_with_bulk_memory_snapshot(*info.memory_p, [&](::std::byte*, ::std::size_t length) constexpr noexcept
                { return ::uwvm2::runtime::compiler::shared::wasm_memory64::range_valid(length, offset, len); });
        ::std::size_t length{};
        return llvm_jit_local_imported_memory_byte_length(info.local_imported_module_ptr, info.local_imported_memory_index, length) &&
            ::uwvm2::runtime::compiler::shared::wasm_memory64::range_valid(length, offset, len);
    }};
    if(!range_valid(source, src) || !range_valid(destination, dst)) { llvm_jit_memory_bridge_trap(); }
    ::std::byte staging[4096];
    // Both full-width ranges fit their native snapshots. Every subrange below
    // is bounded by these proofs before size_t narrowing or pointer addition.
    ::std::size_t remaining{static_cast<::std::size_t>(len)};
    while(remaining != 0uz)
    {
        auto const amount{remaining < sizeof(staging) ? remaining : sizeof(staging)};
        // Choosing memmove direction by offsets is also correct when the providers alias through imports.
        auto const offset{dst > src ? remaining - amount : len - remaining};
        auto const read{[&]() constexpr noexcept -> bool
        {
            if(source.memory_p != nullptr)
                return llvm_jit_with_bulk_memory_snapshot(*source.memory_p, [&](::std::byte* begin, ::std::size_t length) constexpr noexcept
                {
                    if(begin == nullptr || llvm_jit_bulk_memory_range_oob(src + offset, amount, length)) { return false; }
                    // Entire source subrange and fixed staging destination were checked before pointer addition.
                    ::std::memcpy(staging, begin + src + offset, amount);
                    return true;
                });
            return ::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_read(
                source.local_imported_module_ptr, source.local_imported_memory_index, src + offset, staging, amount);
        }};
        if(!read()) { llvm_jit_memory_bridge_trap(); }
        auto const write{[&]() constexpr noexcept -> bool
        {
            if(destination.memory_p != nullptr)
                return llvm_jit_with_bulk_memory_snapshot(*destination.memory_p, [&](::std::byte* begin, ::std::size_t length) constexpr noexcept
                {
                    if(begin == nullptr || llvm_jit_bulk_memory_range_oob(dst + offset, amount, length)) { return false; }
                    // Entire destination subrange and fixed staging source were checked before pointer addition.
                    ::std::memcpy(begin + dst + offset, staging, amount);
                    return true;
                });
            return ::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_write(
                destination.local_imported_module_ptr, destination.local_imported_memory_index, dst + offset, staging, amount);
        }};
        if(!write()) { llvm_jit_memory_bridge_trap(); }
        remaining -= amount;
    }
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

inline constexpr void llvm_jit_cross_provider_memory_copy_bridge(::std::uintptr_t module,
    runtime_wasm_u32 destination_index, runtime_wasm_u32 source_index,
    runtime_wasm_i32 destination, runtime_wasm_i32 source, runtime_wasm_i32 length) noexcept
{
    llvm_jit_wide_cross_provider_memory_copy_bridge(module, destination_index, source_index,
        llvm_jit_wasm_i32_bits_to_u32(destination), llvm_jit_wasm_i32_bits_to_u32(source), llvm_jit_wasm_i32_bits_to_u32(length));
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
            if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_read(
                   local_imported_module, memory_index, src + chunk_offset, staging, chunk_size) ||
               !::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_write(
                   local_imported_module, memory_index, dst + chunk_offset, staging, chunk_size)) [[unlikely]]
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
        if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_read(
               local_imported_module, memory_index, src + copied, staging, chunk_size) ||
           !::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_write(
               local_imported_module, memory_index, dst + copied, staging, chunk_size)) [[unlikely]]
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
        if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_write(
               local_imported_module, memory_index, dst + filled, staging, chunk_size)) [[unlikely]]
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

// Memory64 init consumes an i64 destination but retains i32 source/length.
// The native data-instance identity is translator-owned, never a Wasm pointer.
inline void llvm_jit_memory64_init_bridge(::std::uintptr_t memory_address, ::std::uintptr_t data_address,
    ::std::uint64_t destination, llvm_jit_atomic_address_carrier_t source_carrier,
    llvm_jit_atomic_address_carrier_t length_carrier) noexcept
{
    namespace wide = ::uwvm2::runtime::compiler::shared::wasm_memory64;
    // [live VM memory/data objects] retained by the host execution lease.
    //  ^^ memory/data: only validated native relocations supply these addresses.
    auto const memory{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    auto const data{reinterpret_cast<::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t*>(data_address)};
    if(memory == nullptr || data == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const source{static_cast<::std::uint32_t>(source_carrier)};
    auto const length{static_cast<::std::uint32_t>(length_carrier)};
    auto const payload{::uwvm2::uwvm::runtime::storage::load_wasm_data_segment_payload(data->data)};
    if((payload.byte_begin == nullptr) != (payload.byte_end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
    // [immutable module bytes ...] end, or an empty dropped snapshot.
    //  ^^ begin                   ^^ end: same allocation, stable across drop.
    auto const source_size{payload.byte_begin == nullptr ? 0uz : static_cast<::std::size_t>(payload.byte_end - payload.byte_begin)};
    static_cast<void>(llvm_jit_with_bulk_memory_snapshot(*memory, [&](::std::byte* begin, ::std::size_t size) noexcept
    {
        auto const status{wide::copy(begin, size, payload.byte_begin, source_size, destination, source, length)};
        if(status != wide::bulk_error::none) [[unlikely]]
        {
            bool const source_failed{status == wide::bulk_error::source};
            ::uwvm2::runtime::lib::llvm_jit_memory_out_of_bounds_trap(memory->diagnostic_owner_memory_index, 0u,
                source_failed ? source : destination, 0u, source_failed ? source_size : size, length, 0u, 0u);
            ::fast_io::fast_terminate();
        }
        return true;
    }));
}

inline void llvm_jit_memory64_module_init_bridge(::std::uintptr_t owner, ::std::uintptr_t module_address,
    ::std::uint64_t index, ::std::uint64_t destination, llvm_jit_atomic_address_carrier_t source,
    llvm_jit_atomic_address_carrier_t length) noexcept
{
    // [live runtime module] pinned by the compiled function execution lease.
    // [safe               ] only a trusted relocation supplies module_address.
    auto const module{reinterpret_cast<runtime_module_storage_t*>(module_address)};
    if(module == nullptr || index >= module->local_defined_data_vec_storage.size()) { llvm_jit_memory_bridge_trap(); }
    // [owned data records] end; full-width index comparison precedes native narrowing.
    // [safe             ]
    auto const data{::std::addressof(module->local_defined_data_vec_storage.index_unchecked(static_cast<::std::size_t>(index)))};
    llvm_jit_memory64_init_bridge(owner, reinterpret_cast<::std::uintptr_t>(data), destination, source, length);
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
    auto const payload{::uwvm2::uwvm::runtime::storage::load_wasm_data_segment_payload(data)};
    // [module-owned immutable payload] end, or an empty dropped snapshot.
    // ^^ data_begin; neither borrowed pointer changes during this host entry.
    auto const data_begin{payload.byte_begin};
    auto const data_end{payload.byte_end};
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
    auto const payload{::uwvm2::uwvm::runtime::storage::load_wasm_data_segment_payload(data)};
    // [module-owned immutable payload] end, or an empty dropped snapshot.
    // ^^ data_begin; neither borrowed pointer changes during this host entry.
    auto const data_begin{payload.byte_begin};
    auto const data_end{payload.byte_end};
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

    if(data_begin == nullptr ||
       !::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_write(
           local_imported_module, memory_index, dst, data_begin + src, len)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
        return;
    }
}

#include "opcode/local_imported_memory64_bulk_bridge.h"

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

[[nodiscard]] inline bool llvm_jit_runtime_table_is_funcref(runtime_table_storage_t const& table) noexcept
{
    return ::uwvm2::uwvm::runtime::storage::runtime_table_family(table) ==
           ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::function;
}

[[nodiscard]] inline bool llvm_jit_runtime_table_is_externref(runtime_table_storage_t const& table) noexcept
{
    return ::uwvm2::uwvm::runtime::storage::runtime_table_family(table) ==
           ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::external;
}

[[nodiscard]] inline bool llvm_jit_runtime_table_is_exnref(runtime_table_storage_t const& table) noexcept
{
    return ::uwvm2::uwvm::runtime::storage::runtime_table_family(table) ==
           ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::exception;
}

[[nodiscard]] inline bool llvm_jit_runtime_table_is_gcref(runtime_table_storage_t const& table) noexcept
{
    return ::uwvm2::uwvm::runtime::storage::runtime_table_family(table) ==
           ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::gc;
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
inline void llvm_jit_retain_table_externref(runtime_table_storage_t const* table,
                                          runtime_wasm_externref const& value) noexcept
{
    if(::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(table, value.ref) !=
       ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
    { ::fast_io::fast_terminate(); }
}
inline void llvm_jit_retain_table_extern_payload(runtime_table_storage_t const* table,
                                                 void* payload) noexcept
{
    if(::uwvm2::uwvm::runtime::storage::retain_runtime_table_extern_payload(table, payload) !=
       ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
    { ::fast_io::fast_terminate(); }
}

[[nodiscard]] inline constexpr runtime_wasm_global_ref llvm_jit_exnref_from_table_elem(
    runtime_table_elem_storage_t const& elem) noexcept
{
    using elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
    using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
    runtime_wasm_global_ref result{};
    if(elem.type != elem_type::exn_ref) [[unlikely]]
    {
        // Zero-initialized table slots are null exnrefs. A non-null slot must
        // carry the exn_ref discriminant before its token can be interpreted.
        if(elem.storage.imported_ptr == nullptr)
        { result.kind = ref_kind::wasm_null; return result; }
        ::fast_io::fast_terminate();
    }
    result.storage.ptr = elem.storage.extern_ptr;
    result.kind = elem.storage.extern_ptr == nullptr ? ref_kind::wasm_null : ref_kind::wasm_exn;
    return result;
}

[[nodiscard]] inline constexpr runtime_table_elem_storage_t llvm_jit_table_elem_from_exnref(
    runtime_wasm_global_ref const& reference) noexcept
{
    using elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
    using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
    runtime_table_elem_storage_t result{};
    result.type = elem_type::exn_ref;
    if(reference.kind == ref_kind::wasm_null) { return result; }
    if(reference.kind != ref_kind::wasm_exn || reference.storage.ptr == nullptr) [[unlikely]]
    { ::fast_io::fast_terminate(); }
    result.storage.extern_ptr = reference.storage.ptr;
    return result;
}

inline void llvm_jit_retain_table_exnref(runtime_table_storage_t const* table,
                                         runtime_wasm_global_ref const& reference) noexcept
{
    if(::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(table, reference) !=
       ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
    { ::fast_io::fast_terminate(); }
}

inline void llvm_jit_retain_table_exn_payload(runtime_table_storage_t const* table,
                                              void* payload) noexcept
{
    if(::uwvm2::uwvm::runtime::storage::retain_runtime_table_exn_payload(table, payload) !=
       ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
    { ::fast_io::fast_terminate(); }
}

// Build the native reference object bits directly in SSA. The pointer names stable VM-owned function
// metadata, not guest memory; no frame buffer, pointer dereference or helper call is required.
// Integer bit positions must follow the target's memory order on both 32-bit and 64-bit hosts.
template<typename Reference = runtime_wasm_global_ref>
[[nodiscard]] inline ::llvm::Value* emit_llvm_jit_ref_from_payload(::llvm::IRBuilder<>& builder,
    ::llvm::Value* payload, ::uwvm2::object::global::wasm_ref_kind kind, bool little_endian) noexcept
{
    static_assert(::std::is_standard_layout_v<Reference>);
    constexpr auto bytes{sizeof(Reference)};
    constexpr auto payload_offset{offsetof(Reference, storage)};
    constexpr auto payload_bytes{sizeof(decltype(Reference::storage))};
    constexpr auto tag_offset{offsetof(Reference, kind)};
    constexpr auto tag_bytes{sizeof(decltype(Reference::kind))};
    static_assert(payload_offset <= bytes && payload_bytes <= bytes - payload_offset);
    static_assert(tag_offset <= bytes && tag_bytes <= bytes - tag_offset);
    static_assert(payload_offset + payload_bytes <= tag_offset || tag_offset + tag_bytes <= payload_offset);
    if(payload == nullptr || !payload->getType()->isIntegerTy(static_cast<unsigned>(payload_bytes * CHAR_BIT))) [[unlikely]]
    { return nullptr; }
    auto bits_type{builder.getIntNTy(static_cast<unsigned>(bytes * CHAR_BIT))};
    auto const payload_shift{static_cast<unsigned>((little_endian ? payload_offset : bytes - payload_offset - payload_bytes) * CHAR_BIT)};
    auto const tag_shift{static_cast<unsigned>((little_endian ? tag_offset : bytes - tag_offset - tag_bytes) * CHAR_BIT)};
    // LLVM owns these SSA handles. Neither assignment moves a C++ pointer within an input or runtime allocation.
    auto payload_bits{builder.CreateZExt(payload, bits_type, get_llvm_string_ref(u8"ref.payload.bits"))};
    if(payload_shift) { payload_bits = builder.CreateShl(payload_bits, payload_shift, get_llvm_string_ref(u8"ref.payload.position")); }
    auto const tag{::llvm::APInt{static_cast<unsigned>(bytes * CHAR_BIT), static_cast<::std::uint_least64_t>(kind)}.shl(tag_shift)};
    return builder.CreateOr(payload_bits, ::llvm::ConstantInt::get(bits_type, tag), get_llvm_string_ref(u8"ref.func"));
}

[[nodiscard]] inline constexpr runtime_wasm_i32 llvm_jit_ref_is_null_bridge(::std::uintptr_t ref_address) noexcept
{
    auto ref_ptr{reinterpret_cast<void const*>(ref_address)};
    if(ref_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    runtime_wasm_global_ref ref{};
    ::std::memcpy(::std::addressof(ref), ref_ptr, sizeof(ref));
    return ref.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null ? runtime_wasm_i32{1} : runtime_wasm_i32{};
}

// A reference is held in SSA as its native object bits. Query only its tag: the payload may be zero for a
// non-null host reference, and payload/padding bytes of a null reference need not be zero. The template keeps
// the byte-layout proof attached to the object type; the production caller uses runtime_wasm_global_ref.
// https://webassembly.github.io/spec/core/exec/instructions.html#exec-ref-is-null
template<typename Reference = runtime_wasm_global_ref>
[[nodiscard]] inline ::llvm::Value* emit_llvm_jit_ref_is_null(::llvm::IRBuilder<>& builder,
                                                           ::llvm::Value* reference,
                                                           bool little_endian) noexcept
{
    static_assert(::std::is_standard_layout_v<Reference>);
    constexpr auto bytes{sizeof(Reference)};
    constexpr auto offset{offsetof(Reference, kind)};
    constexpr auto kind_bytes{sizeof(decltype(Reference::kind))};
    static_assert(offset <= bytes && kind_bytes <= bytes - offset);
    if(reference == nullptr || !reference->getType()->isIntegerTy(static_cast<unsigned>(bytes * CHAR_BIT))) [[unlikely]]
    { return nullptr; }
    // Query only tag bits. A known nonzero payload says nothing about Wasm nullness, and a null tag
    // remains null even with nonzero payload/padding. This compile-time proof adds no guest instructions.
    auto const block{builder.GetInsertBlock()};
    auto const module{block == nullptr ? nullptr : block->getModule()};
    if(module != nullptr)
    {
        auto const shift{static_cast<unsigned>((little_endian ? offset : bytes - offset - kind_bytes) * CHAR_BIT)};
        auto const mask{::llvm::APInt::getBitsSet(static_cast<unsigned>(bytes * CHAR_BIT), shift,
            shift + static_cast<unsigned>(kind_bytes * CHAR_BIT))};
        auto const known{::llvm::computeKnownBits(reference, module->getDataLayout())};
        if(!(known.One & mask).isZero()) { return builder.getInt32(0u); }
        if((known.Zero & mask) == mask) { return builder.getInt32(1u); }
    }
    // No guest-memory pointer or input cursor is formed or advanced. The tag is a field of the SSA value.
    auto kind_type{builder.getIntNTy(static_cast<unsigned>(kind_bytes * CHAR_BIT))};
    ::llvm::Value* tag{}; // LLVM owns every SSA handle below, including the replacement in the fallback.
    if constexpr(bytes == 16uz && offset == 8uz && kind_bytes == 4uz)
    {
        // LLVM bitcast maps vector lanes in target memory order. Selecting the field's lane avoids
        // widening an i128 mask into shift/move instructions on x86-64, and preserves all other bits.
        // Keep this measured x86-64 transform target-specific: ARM32/i386 legalization otherwise
        // widens a tag-only memory read into a SIMD load and extract. Other targets keep scalar extraction.
        // This does not request SIMD arithmetic or change the Wasm reference calling convention.
        auto const block{builder.GetInsertBlock()};
        auto const module{block == nullptr ? nullptr : block->getModule()};
        // Native JIT fragments receive their full target triple only at materialization. As with
        // the surrounding target-specific emitters, resolve an absent triple to the native LLVM target.
        ::llvm::Triple target{module == nullptr ? ::llvm::Triple{} : ::llvm::Triple{module->getTargetTriple()}};
        if(module != nullptr && target.getArch() == ::llvm::Triple::UnknownArch)
        { target = ::llvm::Triple{::llvm::sys::getDefaultTargetTriple()}; }
        if(module != nullptr && target.getArch() == ::llvm::Triple::x86_64 &&
           module->getDataLayout().isLittleEndian() == little_endian)
        {
            auto const words_type{::llvm::FixedVectorType::get(kind_type, static_cast<unsigned>(bytes / kind_bytes))};
            auto const words{builder.CreateBitCast(reference, words_type, get_llvm_string_ref(u8"ref.words"))};
            // Offset is aligned and the static layout bound above proves this lane exists.
            tag = builder.CreateExtractElement(words, static_cast<unsigned>(offset / kind_bytes), get_llvm_string_ref(u8"ref.kind"));
        }
    }
    if(tag == nullptr)
    {
        // Arbitrary layouts and a builder without matching target layout retain the integer extraction.
        auto const shift{static_cast<unsigned>((little_endian ? offset : bytes - offset - kind_bytes) * CHAR_BIT)};
        tag = builder.CreateLShr(reference, shift, get_llvm_string_ref(u8"ref.kind.shift"));
        tag = builder.CreateTrunc(tag, kind_type, get_llvm_string_ref(u8"ref.kind"));
    }
    auto is_null{builder.CreateICmpEQ(tag, ::llvm::ConstantInt::get(kind_type,
        static_cast<unsigned>(::uwvm2::object::global::wasm_ref_kind::wasm_null)), get_llvm_string_ref(u8"ref.is_null"))};
    return builder.CreateZExt(is_null, builder.getInt32Ty(), get_llvm_string_ref(u8"ref.is_null.i32"));
}

// Table indices remain Wasm u32; element addresses use the selected table width.
// Keep operands unsigned and full-width through the bounds proof, including on 32-bit hosts.
template<typename Address>
[[nodiscard]] inline constexpr auto llvm_jit_table_address_bits(Address value) noexcept
{ return static_cast<::std::make_unsigned_t<Address>>(value); }
template<typename Offset, typename Length>
[[nodiscard]] inline constexpr bool llvm_jit_table_range_oob(Offset offset, Length length, ::std::size_t bound) noexcept
{ return offset > bound || length > bound - offset; }

template<typename Address = runtime_wasm_i32>
inline constexpr void llvm_jit_table_get_bridge(::std::uintptr_t runtime_module_address,
                                                runtime_wasm_u32 table_index,
                                                Address element_index_i32,
                                                ::std::uintptr_t result_address) noexcept
{
    ::uwvm2::runtime::lib::details::llvm_jit_debug_table_guard debug_table_guard{};
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    auto result_ptr{reinterpret_cast<void*>(result_address)};
    if(runtime_module == nullptr || result_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, table_index)};
    if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const element_index{llvm_jit_table_address_bits(element_index_i32)};
    if(element_index >= table->elems.size()) [[unlikely]] { llvm_jit_table_out_of_bounds_bridge_trap(); }

    if(llvm_jit_runtime_table_is_funcref(*table))
    {
        auto const result{llvm_jit_funcref_from_table_elem(table->elems.index_unchecked(static_cast<::std::size_t>(element_index)))};
        ::std::memcpy(result_ptr, ::std::addressof(result), sizeof(result));
    }
    else if(llvm_jit_runtime_table_is_externref(*table))
    {
        auto const result{llvm_jit_externref_from_table_elem(table->elems.index_unchecked(static_cast<::std::size_t>(element_index)))};
        ::std::memcpy(result_ptr, ::std::addressof(result), sizeof(result));
    }
    else if(llvm_jit_runtime_table_is_exnref(*table))
    {
        auto const result{llvm_jit_exnref_from_table_elem(table->elems.index_unchecked(static_cast<::std::size_t>(element_index)))};
        // A table root belongs to the table owner, which may be a different
        // module and may later clear or retire its table. Transfer a root to
        // this caller before exposing the token in a JIT operand/local.
        if(result.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_exn &&
           ::uwvm2::uwvm::runtime::storage::uwvm2_gc_retain_reference(
               runtime_module->gc_store.get(), ::std::addressof(result)) !=
               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
        { ::fast_io::fast_terminate(); }
        ::std::memcpy(result_ptr, ::std::addressof(result), sizeof(result));
    }
    else if(llvm_jit_runtime_table_is_gcref(*table))
    {
        // [0, table.elems.size()) element_index was proved above. Preserve the
        // complete carrier; an i31 payload must never be read as a pointer.
        auto const result{::uwvm2::uwvm::runtime::storage::runtime_table_slot_to_gc_reference(
            table->elems.index_unchecked(static_cast<::std::size_t>(element_index)))};
        // The caller may keep this value after an imported table is cleared or
        // its provider retires. Transfer a checked root before publication.
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        if(auto* own{::uwvm2::runtime::gc::borrow_actual_sealed_entry(runtime_module_address)})
        {
            bool const local{own->local_defined_table(*runtime_module, table, table_index)};
            if(local && own->authenticates_local_compact(result))
            { ::std::memcpy(result_ptr, &result, sizeof(result)); return; } // current OR older canonical issued range; NO per-step drain
            else if(!local || (result.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_null &&
                result.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_i31))
            { ::uwvm2::runtime::gc::managed_page_boundary(true); } // ORIGINAL foreign/unknown cold path
        }
#endif
#if /* sealed table leaf prepared */ defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
        if(auto* page{::uwvm2::runtime::gc::borrow_actual_managed_page(runtime_module_address)})
        {
            if(page->local_defined_table(*runtime_module, table, table_index))
            {
                if(page->is_pending(result))
                { ::std::memcpy(result_ptr, &result, sizeof(result)); return; }
                // Already published local/null/i31 carriers retain original
                // checked behavior; only foreign/forged objects revoke.
                if(result.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_null &&
                   result.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_i31 &&
                   !page->is_committed_local(result)) { ::uwvm2::runtime::gc::managed_page_boundary(true); }
            }
            else { ::uwvm2::runtime::gc::managed_page_boundary(true); }
        }
#endif
        if(::uwvm2::uwvm::runtime::storage::uwvm2_gc_retain_reference(
               runtime_module->gc_store.get(), ::std::addressof(result)) !=
           ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
        { ::fast_io::fast_terminate(); }
        ::std::memcpy(result_ptr, ::std::addressof(result), sizeof(result));
    }
    else
    {
        ::fast_io::fast_terminate();
    }
}

template<typename Address = runtime_wasm_i32>
inline constexpr void llvm_jit_table_set_bridge(::std::uintptr_t runtime_module_address,
                                                runtime_wasm_u32 table_index,
                                                Address element_index_i32,
                                                ::std::uintptr_t value_address) noexcept
{
    ::uwvm2::runtime::lib::details::llvm_jit_debug_table_guard debug_table_guard{};
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    auto value_ptr{reinterpret_cast<void const*>(value_address)};
    if(runtime_module == nullptr || value_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, table_index)};
    if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const element_index{llvm_jit_table_address_bits(element_index_i32)};
    if(element_index >= table->elems.size()) [[unlikely]] { llvm_jit_table_out_of_bounds_bridge_trap(); }

    if(llvm_jit_runtime_table_is_funcref(*table))
    {
        runtime_wasm_funcref value{};
        ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
        table->elems.index_unchecked(static_cast<::std::size_t>(element_index)) = llvm_jit_table_elem_from_funcref(*runtime_module, value);
        ::uwvm2::runtime::lib::llvm_jit_refresh_call_indirect_table_views(
            table,
            ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_mutation_kind::set,
            element_index,
            1uz);
    }
    else if(llvm_jit_runtime_table_is_externref(*table))
    {
        runtime_wasm_externref value{};
        ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
        llvm_jit_retain_table_externref(table, value);
        table->elems.index_unchecked(static_cast<::std::size_t>(element_index)) = llvm_jit_table_elem_from_externref(value);
    }
    else if(llvm_jit_runtime_table_is_exnref(*table))
    {
        runtime_wasm_global_ref value{};
        ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
        auto const slot{llvm_jit_table_elem_from_exnref(value)};
        llvm_jit_retain_table_exnref(table, value);
        table->elems.index_unchecked(static_cast<::std::size_t>(element_index)) = slot;
    }
    else if(llvm_jit_runtime_table_is_gcref(*table))
    {
        runtime_wasm_global_ref value{};
        ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        if(auto* own{::uwvm2::runtime::gc::borrow_actual_sealed_entry(runtime_module_address)})
        {
            bool const local{own->local_defined_table(*runtime_module, table, table_index)};
            if(local && own->authenticates_local_compact(value))
            { table->elems.index_unchecked(static_cast<::std::size_t>(element_index)) =
                        ::uwvm2::uwvm::runtime::storage::runtime_table_slot_from_gc_reference(value);
                    return; } // current OR older canonical issued range; NO per-step drain
            else if(!local || (value.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_null &&
                value.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_i31))
            { ::uwvm2::runtime::gc::managed_page_boundary(true); } // ORIGINAL foreign/unknown cold path
        }
#endif
#if /* sealed table leaf prepared */ defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
        if(auto* page{::uwvm2::runtime::gc::borrow_actual_managed_page(runtime_module_address)})
        {
            if(page->local_defined_table(*runtime_module, table, table_index))
            {
                if(page->is_pending(value))
                {
                    // Complete already-kind-validated slot copy. Its root will
                    // enter ORIGINAL static census after pre-pause drain.
                    table->elems.index_unchecked(static_cast<::std::size_t>(element_index)) =
                        ::uwvm2::uwvm::runtime::storage::runtime_table_slot_from_gc_reference(value);
                    return;
                }
                if(value.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_null &&
                   value.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_i31 &&
                   !page->is_committed_local(value)) { ::uwvm2::runtime::gc::managed_page_boundary(true); }
            }
            else { ::uwvm2::runtime::gc::managed_page_boundary(true); }
        }
#endif
        auto const slot{::uwvm2::uwvm::runtime::storage::runtime_table_slot_from_gc_reference(value)};
        if(::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(table, value) !=
           ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
        { ::fast_io::fast_terminate(); }
        table->elems.index_unchecked(static_cast<::std::size_t>(element_index)) = slot;
    }
    else
    {
        ::fast_io::fast_terminate();
    }
}

inline constexpr void llvm_jit_elem_drop_bridge(::std::uintptr_t runtime_module_address, runtime_wasm_u32 element_index) noexcept
{
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    ::uwvm2::runtime::gc::managed_page_boundary(false);
#endif

    ::uwvm2::runtime::lib::details::llvm_jit_debug_table_guard debug_table_guard{};
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    if(runtime_module == nullptr || static_cast<::std::size_t>(element_index) >= runtime_module->local_defined_element_vec_storage.size()) [[unlikely]]
    {
        ::fast_io::fast_terminate();
    }

    auto& element{runtime_module->local_defined_element_vec_storage.index_unchecked(static_cast<::std::size_t>(element_index)).element};
    ::uwvm2::uwvm::runtime::storage::drop_wasm_element_segment_payload(element);
}

template<typename Address = runtime_wasm_i32>
inline constexpr void llvm_jit_table_init_bridge(::std::uintptr_t runtime_module_address,
                                                 runtime_wasm_u32 element_index,
                                                 runtime_wasm_u32 table_index,
                                                 Address dst_i32,
                                                 runtime_wasm_i32 src_i32,
                                                 runtime_wasm_i32 len_i32) noexcept
{
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    ::uwvm2::runtime::gc::managed_page_boundary(false);
#endif

    ::uwvm2::runtime::lib::details::llvm_jit_debug_table_guard debug_table_guard{};
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    if(runtime_module == nullptr || static_cast<::std::size_t>(element_index) >= runtime_module->local_defined_element_vec_storage.size()) [[unlikely]]
    {
        ::fast_io::fast_terminate();
    }
    auto table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, table_index)};
    if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    // [checked element index ... module-owned record] retained by execution.
    // Take one visibility snapshot before computing any source range. Concurrent
    // elem.drop publishes an empty instance without rewriting the live pairs.
    auto const element{::uwvm2::uwvm::runtime::storage::load_wasm_element_segment_payload(
        runtime_module->local_defined_element_vec_storage.index_unchecked(static_cast<::std::size_t>(element_index)).element)};
    auto const dst_wide{llvm_jit_table_address_bits(dst_i32)};
    auto const src_wide{llvm_jit_table_address_bits(src_i32)};
    auto const len_wide{llvm_jit_table_address_bits(len_i32)};

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
        if(llvm_jit_table_range_oob(src_wide, len_wide, source_size) || llvm_jit_table_range_oob(dst_wide, len_wide, table->elems.size())) [[unlikely]]
        {
            llvm_jit_table_out_of_bounds_bridge_trap();
        }
        // [source/destination range] lies in live table/segment allocations; narrow only after proof.
        auto const dst{static_cast<::std::size_t>(dst_wide)};
        auto const src{static_cast<::std::size_t>(src_wide)};
        auto const len{static_cast<::std::size_t>(len_wide)};

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
        if(len != 0uz)
        {
            ::uwvm2::runtime::lib::llvm_jit_refresh_call_indirect_table_views(
                table, ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_mutation_kind::init, dst, len);
        }
    }
    else if(llvm_jit_runtime_table_is_externref(*table))
    {
        auto const begin{element.externref_begin};
        auto const end{element.externref_end};
        if((begin == nullptr) != (end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const source_size{begin == nullptr ? 0uz : static_cast<::std::size_t>(end - begin)};
        if(llvm_jit_table_range_oob(src_wide, len_wide, source_size) || llvm_jit_table_range_oob(dst_wide, len_wide, table->elems.size())) [[unlikely]]
        {
            llvm_jit_table_out_of_bounds_bridge_trap();
        }
        // [source/destination range] lies in live table/segment allocations; narrow only after proof.
        auto const dst{static_cast<::std::size_t>(dst_wide)};
        auto const src{static_cast<::std::size_t>(src_wide)};
        auto const len{static_cast<::std::size_t>(len_wide)};

        using elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
        for(::std::size_t i{}; i != len; ++i)
        {
            auto& slot{table->elems.index_unchecked(dst + i)};
            llvm_jit_retain_table_extern_payload(table, begin[src + i]);
            slot.storage.extern_ptr = begin[src + i];
            slot.type = elem_type::extern_ref;
        }
    }
    else if(llvm_jit_runtime_table_is_exnref(*table))
    {
        auto const begin{element.externref_begin};
        auto const end{element.externref_end};
        if((begin == nullptr) != (end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const source_size{begin == nullptr ? 0uz : static_cast<::std::size_t>(end - begin)};
        if(llvm_jit_table_range_oob(src_wide, len_wide, source_size) ||
           llvm_jit_table_range_oob(dst_wide, len_wide, table->elems.size())) [[unlikely]]
        { llvm_jit_table_out_of_bounds_bridge_trap(); }
        // [begin, begin + source_size) and [table.begin, table.end) remain live.
        // [safe                       ] validated ranges prove src+i and dst+i stay inside.
        auto const dst{static_cast<::std::size_t>(dst_wide)};
        auto const src{static_cast<::std::size_t>(src_wide)};
        auto const len{static_cast<::std::size_t>(len_wide)};
        using elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
        for(::std::size_t i{}; i != len; ++i)
        {
            auto const payload{begin[src + i]};
            llvm_jit_retain_table_exn_payload(table, payload);
            auto& slot{table->elems.index_unchecked(dst + i)};
            slot.storage.extern_ptr = payload;
            slot.type = elem_type::exn_ref;
        }
    }
    else if(llvm_jit_runtime_table_is_gcref(*table))
    {
        auto const begin{element.gc_ref_begin};
        auto const end{element.gc_ref_end};
        if((begin == nullptr) != (end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const source_size{begin == nullptr ? 0uz : static_cast<::std::size_t>(end - begin)};
        if(llvm_jit_table_range_oob(src_wide, len_wide, source_size) ||
           llvm_jit_table_range_oob(dst_wide, len_wide, table->elems.size())) [[unlikely]]
        { llvm_jit_table_out_of_bounds_bridge_trap(); }
        // [begin, end) and [table.elems.begin, table.elems.end) are live; both
        // ranges were checked before narrowing or deriving an element address.
        auto const dst{static_cast<::std::size_t>(dst_wide)};
        auto const src{static_cast<::std::size_t>(src_wide)};
        auto const len{static_cast<::std::size_t>(len_wide)};
        for(::std::size_t i{}; i != len; ++i)
        {
            auto const& value{begin[src + i]};
            auto const slot{::uwvm2::uwvm::runtime::storage::runtime_table_slot_from_gc_reference(value)};
            if(::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(table, value) !=
               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
            { ::fast_io::fast_terminate(); }
            table->elems.index_unchecked(dst + i) = slot;
        }
    }
    else
    {
        ::fast_io::fast_terminate();
    }
}

template<typename Destination = runtime_wasm_i32, typename Source = runtime_wasm_i32,
         typename Length = ::std::conditional_t<(sizeof(Destination) == 8u && sizeof(Source) == 8u), runtime_wasm_i64, runtime_wasm_i32>>
inline constexpr void llvm_jit_table_copy_bridge(::std::uintptr_t runtime_module_address,
                                                 runtime_wasm_u32 dst_table_index,
                                                 runtime_wasm_u32 src_table_index,
                                                 Destination dst_i32,
                                                 Source src_i32,
                                                 Length len_i32) noexcept
{
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    ::uwvm2::runtime::gc::managed_page_boundary(false);
#endif

    ::uwvm2::runtime::lib::details::llvm_jit_debug_table_guard debug_table_guard{};
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    if(runtime_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto dst_table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, dst_table_index)};
    auto src_table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, src_table_index)};
    if(dst_table == nullptr || src_table == nullptr || dst_table->table_type_ptr == nullptr || src_table->table_type_ptr == nullptr ||
       ::uwvm2::uwvm::runtime::storage::runtime_table_family(*dst_table) !=
           ::uwvm2::uwvm::runtime::storage::runtime_table_family(*src_table)) [[unlikely]]
    {
        ::fast_io::fast_terminate();
    }

    auto const dst_wide{llvm_jit_table_address_bits(dst_i32)};
    auto const src_wide{llvm_jit_table_address_bits(src_i32)};
    auto const len_wide{llvm_jit_table_address_bits(len_i32)};
    if(llvm_jit_table_range_oob(src_wide, len_wide, src_table->elems.size()) ||
       llvm_jit_table_range_oob(dst_wide, len_wide, dst_table->elems.size())) [[unlikely]]
    {
        llvm_jit_table_out_of_bounds_bridge_trap();
    }
    // [source/destination range] lies in live table/segment allocations; narrow only after proof.
    auto const dst{static_cast<::std::size_t>(dst_wide)};
    auto const src{static_cast<::std::size_t>(src_wide)};
    auto const len{static_cast<::std::size_t>(len_wide)};
    if(len != 0uz)
    {
        if(llvm_jit_runtime_table_is_externref(*dst_table))
        {
            for(::std::size_t i{}; i != len; ++i)
            {
                // [bounded source table range] all slots are VM-owned and live.
                auto const value{llvm_jit_externref_from_table_elem(
                    src_table->elems.index_unchecked(src + i))};
                llvm_jit_retain_table_externref(dst_table, value);
            }
        }
        else if(llvm_jit_runtime_table_is_exnref(*dst_table) && dst_table != src_table)
        {
            for(::std::size_t i{}; i != len; ++i)
            {
                // [src, src + len) was checked against the source table above.
                // [safe          ] index_unchecked borrows only the current live slot.
                auto const value{llvm_jit_exnref_from_table_elem(src_table->elems.index_unchecked(src + i))};
                llvm_jit_retain_table_exnref(dst_table, value);
            }
        }
        else if(llvm_jit_runtime_table_is_gcref(*dst_table) && dst_table != src_table)
        {
            for(::std::size_t i{}; i != len; ++i)
            {
                // [src, src + len) was checked against the source table above.
                auto const value{::uwvm2::uwvm::runtime::storage::runtime_table_slot_to_gc_reference(
                    src_table->elems.index_unchecked(src + i))};
                if(::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(dst_table, value) !=
                   ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                { ::fast_io::fast_terminate(); }
            }
        }
        // [checked dst ... dst + len] and [checked src ... src + len]; nonzero valid allocations.
        // [safe                  ] each derived pointer remains within its table; byte count cannot overflow.
        ::std::memmove(dst_table->elems.data() + dst, src_table->elems.data() + src, len * sizeof(runtime_table_elem_storage_t));
        if(llvm_jit_runtime_table_is_funcref(*dst_table))
        {
            ::uwvm2::runtime::lib::llvm_jit_refresh_call_indirect_table_views(
                dst_table, ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_mutation_kind::copy, dst, len);
        }
    }
}

template<typename Address = runtime_wasm_i32>
[[nodiscard]] inline constexpr Address llvm_jit_table_grow_bridge(::std::uintptr_t runtime_module_address,
                                                                           runtime_wasm_u32 table_index,
                                                                           ::std::uintptr_t value_address,
                                                                           Address delta_i32) noexcept
{
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    ::uwvm2::runtime::gc::managed_page_boundary(false);
#endif

    ::uwvm2::runtime::lib::details::llvm_jit_debug_table_guard debug_table_guard{};
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    auto value_ptr{reinterpret_cast<void const*>(value_address)};
    if(runtime_module == nullptr || value_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, table_index)};
    if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const delta{llvm_jit_table_address_bits(delta_i32)};
    auto const old_size{table->elems.size()};
    Address result{-1};
    constexpr auto host_max{(::std::numeric_limits<::std::size_t>::max)() / sizeof(runtime_table_elem_storage_t)};
    auto const declared_max{table->table_type_ptr->limits.max};
    auto const max_size{declared_max < host_max ? declared_max : host_max};
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
            if(delta != 0uz) { llvm_jit_retain_table_externref(table, value); }
        }
        else if(llvm_jit_runtime_table_is_exnref(*table))
        {
            runtime_wasm_global_ref value{};
            ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
            fill_element = llvm_jit_table_elem_from_exnref(value);
            if(delta != 0uz) { llvm_jit_retain_table_exnref(table, value); }
        }
        else if(llvm_jit_runtime_table_is_gcref(*table))
        {
            runtime_wasm_global_ref value{};
            ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
            fill_element = ::uwvm2::uwvm::runtime::storage::runtime_table_slot_from_gc_reference(value);
            if(delta != 0uz && ::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(table, value) !=
               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
            { ::fast_io::fast_terminate(); }
        }
        else
        {
            ::fast_io::fast_terminate();
        }

        auto const new_size{old_size + static_cast<::std::size_t>(delta)};
        if(!::uwvm2::uwvm::runtime::storage::try_grow_table_elements(*table, new_size, fill_element)) [[unlikely]] { return result; }
        result = static_cast<Address>(old_size);
        if(funcref_table && delta != 0uz)
        {
            ::uwvm2::runtime::lib::llvm_jit_refresh_call_indirect_table_views(
                table, ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_mutation_kind::grow, old_size, static_cast<::std::size_t>(delta));
        }
    }
    return result;
}

template<typename Address = runtime_wasm_i32>
[[nodiscard]] inline constexpr Address llvm_jit_table_size_bridge(::std::uintptr_t runtime_module_address,
                                                                           runtime_wasm_u32 table_index) noexcept
{
    ::uwvm2::runtime::lib::details::llvm_jit_debug_table_guard debug_table_guard{};
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    if(runtime_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, table_index)};
    if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    return static_cast<Address>(table->elems.size());
}

template<typename Address = runtime_wasm_i32>
inline constexpr void llvm_jit_table_fill_bridge(::std::uintptr_t runtime_module_address,
                                                 runtime_wasm_u32 table_index,
                                                 Address dst_i32,
                                                 ::std::uintptr_t value_address,
                                                 Address len_i32) noexcept
{
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    ::uwvm2::runtime::gc::managed_page_boundary(false);
#endif

    ::uwvm2::runtime::lib::details::llvm_jit_debug_table_guard debug_table_guard{};
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    auto value_ptr{reinterpret_cast<void const*>(value_address)};
    if(runtime_module == nullptr || value_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, table_index)};
    if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const dst_wide{llvm_jit_table_address_bits(dst_i32)};
    auto const len_wide{llvm_jit_table_address_bits(len_i32)};
    if(llvm_jit_table_range_oob(dst_wide, len_wide, table->elems.size())) [[unlikely]] { llvm_jit_table_out_of_bounds_bridge_trap(); }
    // [source/destination range] lies in live table/segment allocations; narrow only after proof.
    auto const dst{static_cast<::std::size_t>(dst_wide)};
    auto const len{static_cast<::std::size_t>(len_wide)};

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
        if(len != 0uz) { llvm_jit_retain_table_externref(table, value); }
    }
    else if(llvm_jit_runtime_table_is_exnref(*table))
    {
        runtime_wasm_global_ref value{};
        ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
        fill_element = llvm_jit_table_elem_from_exnref(value);
        if(len != 0uz) { llvm_jit_retain_table_exnref(table, value); }
    }
    else if(llvm_jit_runtime_table_is_gcref(*table))
    {
        runtime_wasm_global_ref value{};
        ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
        fill_element = ::uwvm2::uwvm::runtime::storage::runtime_table_slot_from_gc_reference(value);
        if(len != 0uz && ::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(table, value) !=
           ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
        { ::fast_io::fast_terminate(); }
    }
    else
    {
        ::fast_io::fast_terminate();
    }

    for(::std::size_t i{}; i != len; ++i) { table->elems.index_unchecked(dst + i) = fill_element; }
    if(funcref_table && len != 0uz)
    {
        ::uwvm2::runtime::lib::llvm_jit_refresh_call_indirect_table_views(
            table, ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_mutation_kind::fill, dst, len);
    }
}

// Acquire a validated length snapshot for a local-imported memory. This is used only for memory.size-style queries;
// actual loads/stores stay on bridge functions so provider locks do not outlive the snapshot. In particular, do not
// expose the provider's raw memory pointer after the guarded snapshot callback returns.
[[nodiscard]] inline constexpr ::std::uintptr_t llvm_jit_local_imported_memory_snapshot_bridge(::std::uintptr_t local_imported_module_address,
                                                                                               ::std::size_t memory_index,
                                                                                               ::std::size_t* byte_length_out) noexcept
{
    // C++ bool and narrow-integer return conventions are target-specific. Returning uintptr_t keeps the handwritten
    // LLVM declaration ABI-identical without reproducing per-target zero/sign-extension attributes.
    if(byte_length_out == nullptr) [[unlikely]] { return 0u; }

    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { return 0u; }

    ::uwvm2::runtime::lib::details::local_imported_provider_memory_snapshot_t snapshot{};
    if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_access_snapshot(local_imported_module, memory_index, snapshot)) [[unlikely]]
    {
        return 0u;
    }

    auto const page_size_bytes{
        ::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_page_size(local_imported_module, memory_index)};
    if(!runtime_local_imported_page_size_is_representable(page_size_bytes)) [[unlikely]] { return 0u; }

    auto const page_size_shift{static_cast<unsigned>(::std::countr_zero(page_size_bytes))};
    if(page_size_shift >= ::std::numeric_limits<::std::size_t>::digits) [[unlikely]] { return 0u; }
    if(snapshot.page_count > static_cast<::std::uint_least64_t>(::std::numeric_limits<::std::size_t>::max() >> page_size_shift)) [[unlikely]] { return 0u; }

    *byte_length_out = static_cast<::std::size_t>(snapshot.page_count) << page_size_shift;
    return 1u;
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

// Memory64 native fallback ABI keeps delta/result at i64 on every ISA. The
// address is a compiler-owned native memory record, never a guest pointer.
[[nodiscard]] inline constexpr runtime_wasm_i64
    llvm_jit_memory64_grow_bridge(::std::uintptr_t memory_address, ::std::size_t maximum_bytes, runtime_wasm_i64 delta) noexcept
{
    // [live native memory record] owned for the complete compiled-module lifetime
    // [safe                     ]
    // ^^ memory_p borrows the host record encoded in this bridge's first argument.
    auto const memory_p{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory_p == nullptr) [[unlikely]] { return static_cast<runtime_wasm_i64>(-1); }
    return ::std::bit_cast<runtime_wasm_i64>(::uwvm2::runtime::compiler::shared::wasm_memory64::grow(
        *memory_p, maximum_bytes, static_cast<::std::uint_least64_t>(delta), ::uwvm2::object::memory::flags::grow_strict));
}

[[nodiscard]] inline constexpr runtime_wasm_i64 llvm_jit_memory64_size_bridge(::std::uintptr_t memory_address) noexcept
{
    // [live native memory record] compiler-owned object, never a Wasm offset
    // [safe                     ]
    // ^^ memory_p; lifetime is pinned by the owning compiled module.
    auto const memory_p{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory_p == nullptr) [[unlikely]] { return static_cast<runtime_wasm_i64>(-1); }
    return static_cast<runtime_wasm_i64>(memory_p->get_page_size());
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
    return ::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_try_grow(
               local_imported_module, memory_index, delta_pages, max_limit_memory_length, ::std::addressof(old_pages))
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

    // The host declaration and definition are noexcept: an allocation failure
    // terminates inside the tracing helper instead of unwinding a Wasm frame.
    // This contract applies only to this helper, never to the adjacent guest call.
    static_assert(noexcept(::uwvm2::runtime::lib::llvm_jit_push_call_stack_frame(::std::size_t{}, ::std::size_t{})));
    if(auto const declaration{::llvm::dyn_cast<::llvm::Function>(bridge_pointer)}; declaration != nullptr) { declaration->setDoesNotThrow(); }
    auto const call{apply_llvm_jit_host_calling_conv(
        ir_builder.CreateCall(function_type,
                              bridge_pointer,
                              {::llvm::ConstantInt::get(llvm_size_type, module_id), ::llvm::ConstantInt::get(llvm_size_type, function_index)}))};
    // Also attach the contract to indirect calls on targets that materialize a
    // process-local helper address instead of an external Function declaration.
    call->setDoesNotThrow();
    return true;
}

// Emit the matching runtime call-stack pop before returning from a generated Wasm frame.
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_call_stack_pop(::llvm::IRBuilder<>& ir_builder) noexcept
{
    auto& llvm_context{ir_builder.getContext()};
    auto function_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context), false)};
    auto bridge_pointer{get_llvm_runtime_bridge_function_symbol_value<::uwvm2::runtime::lib::llvm_jit_pop_call_stack_frame>(ir_builder, function_type)};
    if(bridge_pointer == nullptr) [[unlikely]] { return false; }

    // A logical frame pop cannot propagate a native exception, including when
    // used by an exceptional cleanup; retain that exact host ABI in the IR.
    static_assert(noexcept(::uwvm2::runtime::lib::llvm_jit_pop_call_stack_frame()));
    if(auto const declaration{::llvm::dyn_cast<::llvm::Function>(bridge_pointer)}; declaration != nullptr) { declaration->setDoesNotThrow(); }
    auto const call{apply_llvm_jit_host_calling_conv(ir_builder.CreateCall(function_type, bridge_pointer, {}))};
    call->setDoesNotThrow();
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

    // LEB immediate ... code_end
    // [safe consumed bytes] unsafe (could be code_end)
    // ^^ imm_next: preceding bounded scan/lookahead proved a position in this code slice.
    // LEB immediate ... code_end
    // [safe consumed bytes] unsafe (could be code_end)
    // ^^ code_curr: held at the pre-commit position; the right-hand scan proved its target.
    code_curr = reinterpret_cast<::std::byte const*>(imm_next);
    // LEB immediate ... code_end
    // [safe consumed bytes] unsafe (could be code_end)
    //                       ^^ code_curr may be one-past; no read occurs here.
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

#if CHAR_BIT == 8
    // Keep IEEE payloads in integer storage, including signaling NaNs. The byte
    // transport supports unaligned input and constant evaluation without a runtime copy buffer.
    // immediate ... section_end
    // [safe sizeof(UInt) bytes] unsafe (possibly section_end)
    // ^^ code_curr; the remaining-byte check above proves the complete read.
    code_curr = ::fast_io::freestanding::type_punning_from_bytes(code_curr, immediate);
    // immediate ... section_end
    // [safe sizeof(UInt) bytes] unsafe (possibly section_end)
    //                         ^^ code_curr; one-past is permitted after the checked read.
    immediate = ::fast_io::little_endian(immediate);
#else
    // A Wasm octet occupies one storage unit here; native object bytes are not equivalent.
    immediate = 0;
    for(::std::size_t byte_index{}; byte_index != sizeof(UInt); ++byte_index)
    {
        immediate |= static_cast<UInt>(::std::to_integer<::std::uint_least8_t>(code_curr[byte_index])) << (byte_index * 8u);
    }
    // immediate ... section_end
    // [safe sizeof(UInt) bytes] unsafe (possibly section_end)
    // ^^ code_curr; the remaining-byte check above bounds every indexed octet.
    code_curr += sizeof(UInt);
    // immediate ... section_end
    // [safe sizeof(UInt) bytes] unsafe (possibly section_end)
    //                         ^^ code_curr; one-past is permitted after the checked advance.
#endif
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
inline constexpr runtime_operand_stack_value_type llvm_jit_exnref_block_result_arr[]{static_cast<runtime_operand_stack_value_type>(0x69u)};

// Fully resolved Wasm blocktype. `params` are the block-start types and `results` are the end types. Inline blocktypes have
// no params; a non-negative s33 type index borrows both ranges from the runtime type section.
struct runtime_block_signature_type
{
    runtime_block_result_type params{};
    runtime_block_result_type results{};
    // Validation-only provenance for typed block parameters/results. Neither field is emitted to guest code.
    ::std::size_t type_index{(::std::numeric_limits<::std::size_t>::max)()};
    ::std::size_t singleton_result_witness{(::std::numeric_limits<::std::size_t>::max)()};
    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type singleton_result_core_type{};
    bool has_singleton_result_core_type{};
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

// Compile-time catch target. LLVM owns blocks/PHIs; the pinned runtime module
// owns params. No pointer borrows a growable control-stack vector element.
struct llvm_jit_exception_handler_t
{
    bool catch_all{};
    bool with_reference{};
    ::std::size_t tag_index{};
    runtime_block_result_type params{};
    ::llvm::BasicBlock* block{};
    ::uwvm2::utils::container::vector<::llvm::PHINode*> phis{};
    ::std::size_t control_stack_index{};
};

struct llvm_jit_checkpoint_control_prefix_t
{
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> entry{};
    ::uwvm2::utils::container::vector<::llvm::PHINode*> end_phis{};
    ::uwvm2::utils::container::vector<::llvm::PHINode*> loop_phis{};
    ::llvm::BasicBlock* loop_block{};
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
    // Compiler-owned offset into the selected resumable function's saved-if
    // array; no native address or authority is serialized with control DATA.
    ::std::size_t checkpoint_saved_parameter_first{SIZE_MAX};
    llvm_jit_checkpoint_control_prefix_t checkpoint_prefix{};

    // Branch-target stack height before adding labels for this construct.
    ::std::size_t outer_branch_target_stack_size{};

    // Whether the current insertion point inside the construct is reachable.
    bool is_reachable{true};

    // True once at least one branch/fallthrough reaches `end_block`.
    bool end_block_has_incoming{};

    // Lexical clause order is retained; every throwing call searches inner to
    // outer contexts. These are compile-time records, never a TLS handler stack.
    ::uwvm2::utils::container::vector<llvm_jit_exception_handler_t> exception_handlers{};
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

// Resolve already decoded Core 3 reference metadata. The caller owns binary/feature validation;
// this resolver reads no body bytes and independently proves the runtime type-table classification.
// Failure leaves the output unchanged. Its borrowed result tuple is a static one-element array.
[[nodiscard]] inline constexpr bool resolve_decoded_reference_block_signature(
    unsigned carrier,
    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type const& value,
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
    runtime_block_signature_type& block_signature) noexcept
{
    namespace core = ::uwvm2::parser::wasm::standard::wasm3::type;
    if(value.kind != core::value_kind::reference) [[unlikely]] { return false; }
    bool function_index{};
    unsigned expected_carrier{0x70u};
    if(value.heap.is_defined())
    {
        auto const index{static_cast<::std::uint_least64_t>(value.heap.code)};
        if(index > static_cast<::std::uint_least64_t>((::std::numeric_limits<validation_module_traits_t::wasm_u32>::max)()) ||
           index >= get_runtime_type_section_count(runtime_module)) [[unlikely]] { return false; }
        auto const& types{runtime_module.type_section_storage};
        auto const* const context{types.core3_context_ptr};
        // Borrow only immutable parser-owned context; its records are indexed after contains().
        // [0, records.size()) is live; index below is a scalar, never a guest-derived pointer.
        if(context != nullptr)
        {
            if(!context->contains(index)) [[unlikely]] { return false; }
            function_index = context->records.index_unchecked(static_cast<::std::size_t>(index)).kind == core::composite_kind::function;
        }
        else
        {
            if(types.requires_gc || index >= types.type_section_count) [[unlikely]] { return false; }
            function_index = true; // Legacy 0x60-only type tables contain function declarations only.
        }
    }
    else
    {
        using heap = core::abstract_heap_type;
        switch(static_cast<heap>(value.heap.code))
        {
            case heap::func: case heap::nofunc: case heap::any: case heap::eq:
            case heap::i31: case heap::struct_: case heap::array: case heap::none: break;
            case heap::extern_: case heap::noextern: expected_carrier = 0x6fu; break;
            case heap::exn: case heap::noexn: expected_carrier = 0x69u; break;
            default: return false;
        }
    }
    if(carrier != expected_carrier) [[unlikely]] { return false; }
    runtime_block_signature_type resolved{};
    // Each array has exactly one live element. The +1 endpoints below are checked static one-past values.
    // [singleton element] unsafe (one-past)
    // ^^ results.begin   ^^ results.end; no body cursor or object address is advanced here.
    if(carrier == 0x6fu)
    { resolved.results = {llvm_jit_externref_block_result_arr, llvm_jit_externref_block_result_arr + 1u}; }
    else if(carrier == 0x69u)
    { resolved.results = {llvm_jit_exnref_block_result_arr, llvm_jit_exnref_block_result_arr + 1u}; }
    else
    { resolved.results = {llvm_jit_funcref_block_result_arr, llvm_jit_funcref_block_result_arr + 1u}; }
    if(function_index) { resolved.singleton_result_witness = static_cast<::std::size_t>(value.heap.code); }
    else if(value.heap.code == static_cast<::std::int_least64_t>(core::abstract_heap_type::nofunc))
    { resolved.singleton_result_witness = (::std::numeric_limits<::std::size_t>::max)() - 1uz; }
    resolved.singleton_result_core_type = value;
    resolved.has_singleton_result_core_type = true;
    // The local tuple borrows only static arrays; copy all metadata once after every runtime check succeeded.
    block_signature = resolved;
    return true;
}

// Resolve a grammar/feature-checked s33 blocktype without decoding its original bytes again.
// Runtime indices still require the immutable type table's bounds and function-kind proof.
[[nodiscard]] inline constexpr bool resolve_decoded_block_signature(
    ::std::int_least64_t blocktype,
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
    runtime_block_signature_type& block_signature) noexcept
{
    // Scalar tuple endpoints below borrow live static singleton arrays; +1 is their checked one-past endpoint.
    // [singleton element] unsafe (one-past)
    // ^^ results.begin   ^^ results.end; this operation never changes a Wasm byte cursor.
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
            // Preserve raw-parser legacy reference metadata while avoiding a second byte decode.
            namespace core = ::uwvm2::parser::wasm::standard::wasm3::type;
            core::core_value_type const exact{.kind = core::value_kind::reference,
                .heap = {static_cast<::std::int_least64_t>(core::abstract_heap_type::func)},
                .nullable = true, .source_prefix = 0x70u};
            return resolve_decoded_reference_block_signature(0x70u, exact, runtime_module, block_signature);
        }
        case -17:
        {
            // Preserve raw-parser legacy reference metadata while avoiding a second byte decode.
            namespace core = ::uwvm2::parser::wasm::standard::wasm3::type;
            core::core_value_type const exact{.kind = core::value_kind::reference,
                .heap = {static_cast<::std::int_least64_t>(core::abstract_heap_type::extern_)},
                .nullable = true, .source_prefix = 0x6fu};
            return resolve_decoded_reference_block_signature(0x6fu, exact, runtime_module, block_signature);
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
            auto const* const context{runtime_module.type_section_storage.core3_context_ptr};
            if(context != nullptr)
            {
                if(!context->contains(static_cast<::std::uint_least64_t>(blocktype)) ||
                   context->records.index_unchecked(static_cast<::std::size_t>(blocktype)).kind !=
                       ::uwvm2::parser::wasm::standard::wasm3::type::composite_kind::function) [[unlikely]]
                { return false; }
            }
            else if(runtime_module.type_section_storage.requires_gc) [[unlikely]] { return false; }
            // resolve_runtime_type_section_function_type proved this live table element before either member read.
            // [parameter.begin, parameter.end) and [result.begin, result.end) are initialized module-owned tuples.
            // The assignments borrow endpoints only; no tuple element or body byte is read or advanced here.
            block_signature = {.params = {function_type->parameter.begin, function_type->parameter.end},
                               .results = {function_type->result.begin, function_type->result.end},
                               .type_index = static_cast<::std::size_t>(blocktype)};
            return true;
        }
    }
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

    // [control opcode][valtype or s33 index ... code_end)
    // [safe          ] code_curr != code_end above proves the prefix readable.
    auto const value_prefix{::std::to_integer<unsigned>(*code_curr)};
    if(value_prefix == 0x63u || value_prefix == 0x64u ||
       (value_prefix >= 0x69u && value_prefix <= 0x74u))
    {
        // Feature policy is checked by integrated validation; resolution must still bound every byte.
        ::uwvm2::validation::standard::wasm3::recursive_type_context const empty_context{};
        auto const* context_ptr{runtime_module.type_section_storage.core3_context_ptr};
        auto const& context{context_ptr == nullptr ? empty_context : *context_ptr};
        auto const value{::uwvm2::validation::standard::wasm3::scan_core3_value_carrier(
            code_curr, code_end, true, get_runtime_type_section_count(runtime_module), context, true, true)};
        if(value.error != ::uwvm2::validation::standard::wasm3::value_carrier_error::ok) { return false; }
        // [opcode][checked valtype] next ... code_end
        // [safe                  ] unsafe (could be code_end)
        //                          ^^ code_curr after the decoder; the resolver makes no further cursor change.
        return resolve_decoded_reference_block_signature(value.carrier, value.type, runtime_module, block_signature);
    }

    auto const blocktype_begin{code_curr};
    ::std::int_least64_t blocktype{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, blocktype)) [[unlikely]] { return false; }
    auto const blocktype_encoded_size{static_cast<::std::size_t>(code_curr - blocktype_begin)};

    // control_op blocktype ...
    // [       safe       ] unsafe (could be the section_end)
    //                      ^^ code_curr

    if(blocktype_encoded_size > 5uz || (blocktype < 0 && blocktype_encoded_size != 1uz)) [[unlikely]] { return false; }

    return resolve_decoded_block_signature(blocktype, runtime_module, block_signature);
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
    // [validated instruction bytes] code_end
    // [safe                       ] unsafe (could be code_end)
    //    ^^ code_curr: the enclosing decoder proved the entire nonempty slice.
    code_curr = code_end;

    // instruction ...
    // [  safe   ] unsafe (could be the section_end)
    //             ^^ code_curr / code_end
    return true;
}

// Mutable state for lowering one local Wasm function to LLVM IR.  This object intentionally stores all transient stacks
// and LLVM handles so opcode include files can share a compact emitter API without global state.
struct llvm_jit_gc_root_store_t
{
    ::llvm::WeakTrackingVH value{};
    ::std::size_t index{};
};
struct llvm_jit_gc_root_snapshot_t
{
    ::llvm::WeakTrackingVH anchor{};
    ::uwvm2::utils::container::vector<llvm_jit_gc_root_store_t> stores{};
};

struct llvm_jit_gc_root_frame_emit_state_t
{
    ::llvm::AllocaInst* frame{};
    ::llvm::AllocaInst* slots{};
    ::llvm::CallInst* enter{};
    ::std::size_t max_slots{};
    ::uwvm2::utils::container::vector<llvm_jit_gc_root_store_t> pending_root_stores{};
    ::uwvm2::utils::container::vector<llvm_jit_gc_root_snapshot_t> snapshots{};
    // Compiler-owned handles, used only to remove an exactly empty record
    // after all normal, exceptional and OSR edges have been emitted.
    mutable ::uwvm2::utils::container::vector<::llvm::Instruction*> instructions{};
};

#include "single_func_checkpoint_resume_state.h"
#include "single_func_checkpoint_observer_control_map.h"

struct runtime_local_func_llvm_jit_emit_state_t
{
    // True after preparation succeeds and before finalization consumes the control stack.
    bool valid{};

    // Enables expensive LLVM verifier checks on generated functions/modules.
    bool verify_llvm_jit_ir{default_verify_llvm_jit_ir};

    // Forces Wasm calls through the runtime raw bridge instead of direct typed declarations.
    bool route_wasm_calls_through_runtime_bridge{};

    // Only the original fused admission factory stages cold import alternatives.
    // Default full/ROS keeps this false and creates no additional LLVM IR.
    bool stage_retained_unwind_import_routes{};

    // Compiler-owned mode identity, copied by the genuine preparation path.
    // Lazy and tiered can share route shapes, so flags cannot substitute for it.
    llvm_jit_compilation_mode compilation_mode{llvm_jit_compilation_mode::unspecified};

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

    bool emit_precise_gc_root_frames{};
    llvm_jit_gc_root_frame_emit_state_t gc_root_frame{};
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
    ::llvm::Value* sealed_compact_cursor{}; // private native SSA, NEVER a guest/reference operand
#endif

    // Enabled only after explicit standalone-full provenance checks.
    bool emit_debug_safe_points{};
    // Debug-full-only LLVM-owned synthetic line-table scope. Never guest DWARF/locals authority.
    ::llvm::DISubprogram* debug_native_provenance{};
    ::llvm::SmallVector<::uwvm2::runtime::compiler::llvm_jit::native_provenance::numeric_identity, 0u>
        debug_native_numeric_identities{};
    // Compiler-only numeric definitions. A private shutdown successor may
    // preserve this cache; distinct Wasm CFG blocks must refresh their values.
    // Tracking handles follow PHI RAUW/deletion and never authorize a register.
    struct debug_numeric_definition
    {
        ::llvm::WeakTrackingVH value{};
        unsigned kind{};
    };
    ::std::vector<debug_numeric_definition> debug_native_numeric_definitions{};
    ::llvm::BasicBlock* debug_native_numeric_block{};
    unsigned debug_native_numeric_line{};
    bool debug_activation_enabled{};
    ::std::uint64_t debug_compiled_function_generation{1u};
    ::llvm::Value* debug_activation_token{}; // private entry witness; not a guest carrier
    // Host-owned, immutable-extent table; slot contents use release/acquire publication.
    ::std::uintptr_t debug_full_patchable_typed_target_base_address{};
    ::std::size_t debug_full_patchable_typed_target_count{};
    llvm_jit_debug_safe_point_granularity debug_safe_point_granularity{llvm_jit_debug_safe_point_granularity::entry_loop};
    ::std::size_t last_debug_instruction_offset{SIZE_MAX};
    // Borrowed from this function's debug-only validation result; pre-sized
    // before any noexcept opcode emitter can set an already-bounded bit.
    ::uwvm2::utils::container::vector<::std::uint_least8_t>* debug_safe_point_bits{};

    // Compiler-owned plan and LLVM argument handles; no guest pointer.
    ::uwvm2::runtime::exception::pending_experiment::numeric_entry::compile_plan const* pending_numeric_plan{};
    ::llvm::Argument* pending_numeric_context{};

    bool native_guest_exceptions{};
    ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::declarations native_exception_imports{};
    ::uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad::catch_runtime native_exception_runtime{};

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
    ::llvm::BasicBlock* self_tail_entry_block{};
    ::llvm::BasicBlock* tiered_core_normal_init_block{};

    // Extra tiered-core arguments: reentry id and pointer to serialized local storage.
    ::llvm::Argument* tiered_core_entry_id_arg{};
    ::llvm::Argument* tiered_core_local_base_arg{};

    // Primary IRBuilder for body emission.
    ::uwvm2::utils::container::delete_owned_ptr<::llvm::IRBuilder<>> ir_builder{};

    // Entry-block allocas for every local slot.
    ::uwvm2::utils::container::vector<::llvm::AllocaInst*> local_pointers{};
    // Debug-only, frame-owned slots. Each captured Wasm local occupies 16 bytes;
    // this alloca and every snapshot store are absent from ordinary LLVM full.
    ::llvm::ArrayType* debug_local_snapshot_type{};
    ::llvm::StructType* debug_local_snapshot_packet_type{};
    ::llvm::AllocaInst* debug_local_snapshot{};
    llvm_jit_debug_local_initialization_query debug_local_initialization{};
    // Bounded private descriptor table and shared snapshot bodies. No Wasm
    // identity, source value or ASM authority is stored in these lexical cells.
    ::llvm::Value* snapshot_local_pointers{}; // entry alloca or activation-owned private heap table
    ::std::size_t snapshot_local_pointers_count{};
    ::llvm::Function* debug_local_snapshot_copy{};
    ::std::size_t debug_local_snapshot_copy_count{};
    ::llvm::Function* checkpoint_local_snapshot_copy{};
    ::std::size_t checkpoint_local_snapshot_copy_count{};
    ::llvm::DenseMap<::llvm::Constant*, ::llvm::GlobalVariable*> debug_local_snapshot_flags{};
    // Compiler lexical borrow only. The final sealed plan is independently
    // owned by the actual runtime publication, never this mutable builder.
    ::uwvm2::runtime::checkpoint::function_plan* checkpoint_plan{};
    // Only the selected immutable checkpoint engine owns these native entry
    // allocations. Ordinary emit state remains null and emits no flag/packet IR.
    ::llvm::AllocaInst* checkpoint_executed_local_flags{};
    ::llvm::AllocaInst* checkpoint_packet_values{};
    ::llvm::AllocaInst* checkpoint_saved_control_storage{};
    ::std::size_t checkpoint_saved_control_max_slots{};
    ::llvm::AllocaInst* checkpoint_packet_flags{};
    ::llvm::Value* checkpoint_observer_workspace{}; // activation-owned heap, observer profile only
    ::llvm::CallInst* checkpoint_observer_workspace_allocation{}; // final constant extent, before optimization
    // Compiler-only numeric packet witnesses for the actual fused observer.
    // Earlier stores dominate this append-only block, including the explicitly
    // proven private shutdown successor. Wasm CFG/owner changes invalidate it.
    // No guest cache, runtime branch, snapshot address or resume rights.
    ::llvm::BasicBlock* checkpoint_observer_nonlocal_block{};
    ::llvm::Value* checkpoint_observer_nonlocal_workspace{};
    ::llvm::Instruction* checkpoint_observer_nonlocal_anchor{};
    ::std::size_t checkpoint_observer_nonlocal_locals{};
    ::std::vector<::llvm::Value*> checkpoint_observer_nonlocals{};
    ::std::uint64_t checkpoint_current_site{};
    llvm_jit_checkpoint_resume_dispatch_emit_state checkpoint_resume{};
    llvm_jit_checkpoint_call_emit_state checkpoint_call{};
    // Lexical observer compiler borrows only; cleared before this walk returns.
    llvm_jit_checkpoint_observer_control_map* checkpoint_observer_controls{};
    llvm_jit_checkpoint_observer_site_query checkpoint_observer_site{};

    // Cached resolution for default memory 0.
    validation_module_traits_t::wasm_u32 current_memory_index{};
    runtime_memory_access_info_t selected_memory_access_info{};
    bool selected_memory_access_info_resolved{};

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
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    // A compile-local observer borrow, not a runtime entry or guest capability.
    native_eh_leaf_observer::function_state* native_eh_leaf_work{};
#endif

    // Nested structured-control depth being skipped inside an unreachable context.
    ::std::size_t unreachable_control_depth{};
};

#include "single_func_checkpoint_control_storage_emit.h"
#include "single_func_gc_roots_emit.h"

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
                                                                                bool emit_unwind_call_stack_frames = false,
                                                                                [[maybe_unused]] ::llvm::TargetMachine const* native_exception_target_machine = nullptr) noexcept
{
    static_cast<void>(emit_unwind_call_stack_frames);
    module_storage = {};
    module_storage.llvm_context_holder = ::uwvm2::utils::container::make_delete_owned<::llvm::LLVMContext>();
    if(module_storage.llvm_context_holder == nullptr) [[unlikely]] { return false; }

    auto const llvm_module_name{get_llvm_wasm_ir_module_name(runtime_module)};
    module_storage.llvm_module =
        ::uwvm2::utils::container::make_delete_owned<::llvm::Module>(get_llvm_string_ref(llvm_module_name), *module_storage.llvm_context_holder);
    // Address widening and SIMD lane byte order are decided during IR emission, before MCJIT installs its complete
    // target-machine layout. An empty LLVM layout defaults to little-endian/64-bit, which is wrong on BE and ISA32.
    // This is a native JIT; publish the native ABI essentials now, and replace them with the full layout at materialization.
    if(module_storage.llvm_module != nullptr)
    {
        constexpr bool little{::std::endian::native == ::std::endian::little};
        if constexpr(sizeof(::std::uintptr_t) == 8uz) { module_storage.llvm_module->setDataLayout(little ? "e-p:64:64" : "E-p:64:64"); }
        else { module_storage.llvm_module->setDataLayout(little ? "e-p:32:32" : "E-p:32:32"); }
    }
    if(module_storage.llvm_module == nullptr) [[unlikely]] { return false; }

#ifdef UWVM_CPP_EXCEPTIONS
    if(native_exception_target_machine != nullptr)
    {
        // [runtime-owned target machine] borrow only until the compilation tasks
        // [safe                        ] join; the runtime establishes that lifetime.
        auto& module{*module_storage.llvm_module};
# if LLVM_VERSION_MAJOR >= 21
        module.setTargetTriple(native_exception_target_machine->getTargetTriple());
# else
        module.setTargetTriple(native_exception_target_machine->getTargetTriple().str());
# endif
        module.setDataLayout(native_exception_target_machine->createDataLayout());
        module_storage.native_exception_imports =
            ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::declare_itanium_dwarf_symbols(
                module, *native_exception_target_machine);
    }
#endif

    return true;
}

// Only metadata is produced here: this compiler query performs no extra
// bytecode scan and emits no runtime callback/check on ordinary full code.
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_native_provenance(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t wasm_offset) noexcept
{
    if(!state.emit_debug_safe_points) { return true; }
    if(state.ir_builder == nullptr || state.debug_native_provenance == nullptr || state.local_func_storage_ptr == nullptr)
    { return false; }
    auto const& local{*state.local_func_storage_ptr};
    auto const begin{reinterpret_cast<::std::uintptr_t>(local.code_begin)};
    auto const end{reinterpret_cast<::std::uintptr_t>(local.code_end)};
    // [host-pinned expression begin ... end] end is never dereferenced
    // [safe                               ] prove integer extent before offset encoding;
    //  ^^ no byte cursor moves or bytecode is read by this metadata operation.
    if(begin == 0u || end <= begin) { return false; }
    return ::uwvm2::runtime::compiler::llvm_jit::native_provenance::location(
        *state.ir_builder, state.debug_native_provenance, wasm_offset, end - begin);
}

// Helpers borrow only actual emit-state/LLVM owners. No shared guest ABI or
// ordinary code path selects these packet/flag operations when plan is null.
#include "single_func_checkpoint_initialization_emit.h"
#include "single_func_snapshot_copy_emit.h"
#include "single_func_checkpoint_packet_emit.h"
#include "single_func_checkpoint_opcode_emit.h"
#include "single_func_checkpoint_resume_landing_emit.h"
#include "single_func_checkpoint_resume_entry_emit.h"
#include "single_func_checkpoint_call_emit.h"

// Defined after the qualified native exception/cleanup emitter below.
// Ordinary/null and observation profiles return without retirement IR.
[[nodiscard]] inline bool emit_runtime_local_func_llvm_jit_checkpoint_retirement_poll(
    runtime_local_func_llvm_jit_emit_state_t&, ::std::size_t opcode_offset) noexcept;

[[nodiscard]] inline bool emit_runtime_local_func_llvm_jit_debug_shutdown_poll(
    runtime_local_func_llvm_jit_emit_state_t&, ::std::size_t opcode_offset, ::llvm::CallBase* actual_safe_point) noexcept;

// Record only the actual compiler-owned, validator-typed numeric SSA stack.
// Called again after a successfully lowered opcode so its result lives in the
// same lexical scope as the actual numeric instruction, before the next poll.
[[nodiscard]] inline bool emit_runtime_local_func_llvm_jit_native_numeric_values(
    runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(!state.emit_debug_safe_points || state.debug_native_provenance == nullptr) { return true; }
    if(state.ir_builder == nullptr || state.ir_builder->GetInsertBlock() == nullptr ||
       llvm_jit_basic_block_has_terminator(state.ir_builder->GetInsertBlock()) ||
       !state.ir_builder->getCurrentDebugLocation() || state.ir_builder->getCurrentDebugLocation().getLine() == 0u) { return true; }
    if(state.debug_native_provenance != nullptr)
    {
        auto& ir{*state.ir_builder};
        auto* const block{ir.GetInsertBlock()};
        auto const line{ir.getCurrentDebugLocation().getLine()};
        bool const append_only{ir.GetInsertPoint() == block->end()};
        auto& definitions{state.debug_native_numeric_definitions};
        if(!append_only || state.debug_native_numeric_block != block) { definitions.clear(); state.debug_native_numeric_line = 0u; }
        auto const count{(::std::min)(state.operand_stack.size(), ::std::size_t{128u})};
        definitions.resize(count);
        // Refresh the actual changed cells. If the numeric tuple is unchanged,
        // keep one top witness at a new opcode; never manufacture availability
        // for the retained prefix after a cooperative host call spilled it.
        ::std::size_t top{SIZE_MAX};
        for(::std::size_t i{}; i != count; ++i)
        {
            auto const kind{get_runtime_wasm_value_type_encoding(state.operand_stack.index_unchecked(i).type)};
            if(kind == 0x7fu || kind == 0x7eu || kind == 0x7du || kind == 0x7cu || kind == 0x7bu) { top = i; }
        }
        bool changed{};
        for(::std::size_t i{}; i != count; ++i)
        {
            auto const& actual{state.operand_stack.index_unchecked(i)};
            auto const kind{get_runtime_wasm_value_type_encoding(actual.type)};
            if((kind == 0x7fu || kind == 0x7eu || kind == 0x7du || kind == 0x7cu || kind == 0x7bu) &&
               (static_cast<::llvm::Value*>(definitions[i].value) != actual.value || definitions[i].kind != kind))
            { changed = true; }
        }
        for(::std::size_t i{}; i != count; ++i)
        {
            auto const& actual{state.operand_stack.index_unchecked(i)};
            auto* numeric_value{actual.value};
            ::llvm::Instruction* register_witness{};
            // The fused validator already owns actual.type/value. References
            // never enter the numeric register materializer, including v128-
            // sized private carriers whose Wasm type is a reference.
            auto const kind{get_runtime_wasm_value_type_encoding(actual.type)};
            bool const retained{append_only && static_cast<::llvm::Value*>(definitions[i].value) == actual.value && definitions[i].kind == kind};
            if(retained && !(i == top && !changed && state.debug_native_numeric_line != line)) { continue; }
            if(kind == 0x7fu || kind == 0x7eu || kind == 0x7du || kind == 0x7cu || kind == 0x7bu)
            {
                // First qualify the actual pure numeric definitions. The
                // exact compiler-owned numeric identity has the same numeric
                // code origin; memory operands remain hidden by the public MC
                // projection. The second record tracks the real register result.
                // Keep a whole v128 result at its genuine definition, before
                // a later snapshot store changes its DWARF location to a spill.
                // The separate consuming identity still proves marker liveness.
                // Scalar expansions retain the completed-marker-only contract.
                if(!::uwvm2::runtime::compiler::llvm_jit::native_provenance::numeric(
                    *state.ir_builder, actual.value, kind, i, kind == 0x7bu)) { return false; }
                numeric_value = ::uwvm2::runtime::compiler::llvm_jit::native_provenance::materialize_numeric_register(
                    *state.ir_builder, actual.value, &register_witness);
                if(numeric_value == nullptr) { return false; }
            }
            // This register identity belongs only to the current native debug
            // location. Keep the semantic Wasm SSA value: an if/loop prefix
            // can survive this block and must dominate the sibling/merge uses.
            // Replacing that prefix with a block-local identity breaks valid
            // Wasm IR even though the identity preserves its numeric bits.
            if(!::uwvm2::runtime::compiler::llvm_jit::native_provenance::numeric(*state.ir_builder, numeric_value,
                get_runtime_wasm_value_type_encoding(actual.type), i, true, register_witness)) { return false; }
            if(kind == 0x7bu && numeric_value != actual.value)
            {
                auto* const semantic{::llvm::dyn_cast<::llvm::Instruction>(
                    state.ir_builder->CreateBitCast(numeric_value, actual.value->getType()))};
                if(semantic == nullptr) { return false; }
                state.debug_native_numeric_identities.push_back({actual.value, semantic});
            }
            definitions[i] = {actual.value,kind};
        }
        state.debug_native_numeric_block = append_only ? block : nullptr;
        state.debug_native_numeric_line = append_only ? line : 0u;
    }
    return true;
}

// Emit a cooperative full-JIT debug point. Bounds are checked in the compiler;
// only three scalar source identities enter generated code, never native pointers.
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_debug_safe_point(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t wasm_expr_byte_offset) noexcept
{
    if(state.ir_builder == nullptr || state.local_func_storage_ptr == nullptr ||
       !emit_runtime_local_func_llvm_jit_native_provenance(state, wasm_expr_byte_offset)) [[unlikely]] { return false; }
    auto& builder{*state.ir_builder};
    ::uwvm2::runtime::compiler::llvm_jit::native_provenance::unknown(builder, state.debug_native_provenance);
    auto const& local{*state.local_func_storage_ptr};
    // [validated expression bytes ...] | code_end
    // [safe                          ] | unsafe (one-past)
    //  ^^ code_begin
    // These borrowed endpoints share the pinned function body's allocation.
    // Integer extent checks avoid creating or advancing any source pointer.
    if(local.code_begin == nullptr || local.code_end == nullptr) [[unlikely]] { return false; }
    auto const begin_address{reinterpret_cast<::std::uintptr_t>(local.code_begin)};
    auto const end_address{reinterpret_cast<::std::uintptr_t>(local.code_end)};
    if(end_address <= begin_address || wasm_expr_byte_offset >= end_address - begin_address) [[unlikely]] { return false; }
    if(state.debug_safe_point_bits == nullptr ||
       wasm_expr_byte_offset / 8u >= state.debug_safe_point_bits->size()) [[unlikely]] { return false; }
    // This current opcode site was staged from the same fused validator's
    // exact semantic stack before it pops/merges. Copy actual execution values
    // synchronously before the genuine cooperative park callback can run.
    if(!emit_runtime_local_func_llvm_jit_checkpoint_current_opcode(state, wasm_expr_byte_offset))
    [[unlikely]] { return false; }
    auto const integer{::llvm::Type::getIntNTy(builder.getContext(), static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto const captured_count{(::std::min)(state.local_pointers.size(),
        ::uwvm2::runtime::lib::details::llvm_jit_debug_max_captured_locals)};
    ::llvm::Value* snapshot_address{::llvm::ConstantInt::get(integer, 0u)};
    if(captured_count != 0u)
    {
        if(state.debug_local_snapshot == nullptr || state.debug_local_snapshot_type == nullptr ||
           state.debug_local_snapshot_packet_type == nullptr || captured_count > state.local_types.size() ||
           state.debug_local_initialization.context == nullptr || state.debug_local_initialization.initialized == nullptr)
        [[unlikely]] { return false; }
        auto const flags{prepare_runtime_local_func_llvm_jit_debug_snapshot_flags(state,captured_count)};
        auto const saved_flags{builder.CreateInBoundsGEP(state.debug_local_snapshot_packet_type,state.debug_local_snapshot,
            {builder.getInt32(0u),builder.getInt32(1u)})};
        if(flags == nullptr || saved_flags == nullptr ||
           !emit_runtime_local_func_llvm_jit_snapshot_copy(state,captured_count,state.debug_local_snapshot,
               saved_flags,flags,true)) [[unlikely]] { return false; }
        snapshot_address = builder.CreatePtrToInt(state.debug_local_snapshot, integer);
        if(snapshot_address == nullptr) [[unlikely]] { return false; }
    }
    auto const type{::llvm::FunctionType::get(builder.getVoidTy(), {integer, integer, integer, integer, integer}, false)};
    auto const bridge{get_llvm_runtime_bridge_function_symbol_value<::uwvm2::runtime::lib::details::llvm_jit_debug_safe_point_abi_bridge>(builder, type)};
    if(bridge == nullptr) [[unlikely]] { return false; }
    static_assert(noexcept(::uwvm2::runtime::lib::details::llvm_jit_debug_safe_point_abi_bridge({}, {}, {}, {}, {})));
    if(auto const declaration{::llvm::dyn_cast<::llvm::Function>(bridge)}; declaration != nullptr) { declaration->setDoesNotThrow(); }
    auto const call{apply_llvm_jit_host_calling_conv(builder.CreateCall(type, bridge,
        {::llvm::ConstantInt::get(integer, local.module_id), ::llvm::ConstantInt::get(integer, local.function_index),
         ::llvm::ConstantInt::get(integer, wasm_expr_byte_offset), snapshot_address,
         ::llvm::ConstantInt::get(integer, captured_count)}))};
    // The host may cooperatively block and touches synchronization state. Keep
    // its side effects; nounwind does not imply readonly, nosync or speculatable.
    call->setDoesNotThrow();
    // Attach provenance to the call itself, not to a module-level side table:
    // LLVM's optimizer may remove a constant-dead branch. A later cold
    // full-materialization pass rebuilds the bitmap only from surviving
    // bridge calls, including calls copied by LLVM transformations.
    auto const safe_point_identity{::llvm::MDNode::get(builder.getContext(),
        {::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(builder.getInt64Ty(), local.module_id)),
         ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(builder.getInt64Ty(), local.function_index)),
         ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(builder.getInt64Ty(), wasm_expr_byte_offset))})};
    call->setMetadata("uwvm.debug.safe_point", safe_point_identity);
    // [one bitmap byte for this checked expression offset] bitmap_end
    // [safe                                              ] unsafe (one-past)
    //  ^^ offset / 8 was checked above; setting a bit changes no code cursor.
    auto& bit_byte{state.debug_safe_point_bits->index_unchecked(wasm_expr_byte_offset / 8u)};
    bit_byte = static_cast<::std::uint_least8_t>(bit_byte | (1u << (wasm_expr_byte_offset % 8u)));
    // The existing observer/pause/native-step bridge returned normally first.
    // A distinct potentially throwing Invoke owns native cleanup, never the
    // old noexcept/naked wrapper and never a Wasm typed exception handler.
    if(!emit_runtime_local_func_llvm_jit_debug_shutdown_poll(state, wasm_expr_byte_offset, call)) { return false; }
    if(!emit_runtime_local_func_llvm_jit_checkpoint_retirement_poll(state, wasm_expr_byte_offset)) { return false; }
    if(!emit_runtime_local_func_llvm_jit_native_provenance(state, wasm_expr_byte_offset)) { return false; }
    if(!emit_runtime_local_func_llvm_jit_native_numeric_values(state)) { return false; }
    return true;
}

// Exact entry witness. The opaque i64 remains native SSA and never becomes a
// Wasm local/reference/address. Ordinary full emits no call or argument here.
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_debug_activation_enter(
    runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(!state.debug_activation_enabled) { return true; }
    if(state.ir_builder == nullptr || state.local_func_storage_ptr == nullptr || state.debug_activation_token != nullptr ||
       state.debug_compiled_function_generation == 0u) { return false; }
    auto& builder{*state.ir_builder};
    auto const integer{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto const type{::llvm::FunctionType::get(builder.getInt64Ty(), {integer, integer, builder.getInt64Ty()}, false)};
    auto const bridge{state.checkpoint_plan == nullptr ? get_llvm_runtime_bridge_function_symbol_value<
        ::uwvm2::runtime::lib::details::llvm_jit_debug_activation_enter_abi_bridge>(builder, type) :
        get_llvm_runtime_bridge_function_symbol_value_unwrapped<
        ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_activation_enter_abi_bridge>(builder, type)};
    if(bridge == nullptr) { return false; }
    if(auto const declaration{::llvm::dyn_cast<::llvm::Function>(bridge)}; declaration != nullptr) { declaration->setDoesNotThrow(); }
    auto const& local{*state.local_func_storage_ptr};
    // [LLVM function-owned opaque SSA] end
    // [safe                         ] borrowed only while this function/context lives.
    //  ^^ assign the real entry result, never a source/PC/CFA inference.
    state.debug_activation_token = apply_llvm_jit_host_calling_conv(builder.CreateCall(type, bridge,
        {::llvm::ConstantInt::get(integer, local.module_id), ::llvm::ConstantInt::get(integer, local.function_index),
         ::llvm::ConstantInt::get(builder.getInt64Ty(), state.debug_compiled_function_generation)}));
    if(state.debug_activation_token == nullptr) { return false; }
    ::llvm::cast<::llvm::CallInst>(state.debug_activation_token)->setDoesNotThrow();
    return true;
}
// kind: 0 return, 1 checked typed tail, 2 checked host tail, 3 frame-exiting EH.
// Emit BEFORE musttail, never between that call and its immediate return.
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_debug_activation_leave(
    ::llvm::IRBuilder<>& builder, runtime_local_func_llvm_jit_emit_state_t const& state, unsigned kind) noexcept
{
    if(!state.debug_activation_enabled) { return true; }
    if(state.debug_activation_token == nullptr || kind > 3u) { return false; }
    auto const integer{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    if(state.checkpoint_observer_workspace != nullptr)
    {
        auto const release_type{::llvm::FunctionType::get(builder.getVoidTy(), {integer}, false)};
        auto const release{get_llvm_runtime_bridge_function_symbol_value_unwrapped<
            ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_observer_workspace_leave_abi_bridge>(builder, release_type)};
        if(release == nullptr) { return false; }
        auto const retired{apply_llvm_jit_host_calling_conv(builder.CreateCall(release_type, release,
            {builder.CreatePtrToInt(state.checkpoint_observer_workspace, integer)}))};
        retired->setDoesNotThrow();
    }
    auto const type{::llvm::FunctionType::get(builder.getVoidTy(), {builder.getInt64Ty(), integer}, false)};
    auto const bridge{state.checkpoint_plan == nullptr ? get_llvm_runtime_bridge_function_symbol_value<
        ::uwvm2::runtime::lib::details::llvm_jit_debug_activation_leave_abi_bridge>(builder, type) :
        get_llvm_runtime_bridge_function_symbol_value_unwrapped<
        ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_activation_leave_abi_bridge>(builder, type)};
    if(bridge == nullptr) { return false; }
    if(auto const declaration{::llvm::dyn_cast<::llvm::Function>(bridge)}; declaration != nullptr) { declaration->setDoesNotThrow(); }
    auto const call{apply_llvm_jit_host_calling_conv(builder.CreateCall(type, bridge,
        {state.debug_activation_token, ::llvm::ConstantInt::get(integer, kind)}))};
    if(call == nullptr) { return false; }
    call->setDoesNotThrow(); // side-effecting TLS maintenance; no readonly/nosync/speculatable promise.
    return true;
}

// Source-level instruction points are a compile-time choice. Inline opcode
// handlers can delegate to the generic emitter; this offset deduplication keeps
// one IR call per source opcode and adds no guest-execution state or checks.
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(
    runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(!state.emit_debug_safe_points || state.debug_safe_point_granularity != llvm_jit_debug_safe_point_granularity::instruction)
    { return true; }
    if(!state.valid || state.ir_builder == nullptr || state.local_func_storage_ptr == nullptr || state.control_stack.empty())
        [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.current_wasm_op_offset == SIZE_MAX) [[unlikely]] { return false; }
    if(state.last_debug_instruction_offset == state.current_wasm_op_offset) { return true; }
    if(state.checkpoint_observer_site.stage != nullptr &&
       !state.checkpoint_observer_site.stage(state.checkpoint_observer_site.context,state)) { return false; }
    // Restore exact logical values before publishing roots or stopping.
    // Normal null-profile compilation emits no landing or selector IR.
    if(!emit_runtime_local_func_llvm_jit_checkpoint_current_landing(state)) { return false; }
    // [current typed locals and operand SSA values] instruction boundary
    // [safe                                      ] the callback can park or
    // reenter the host. Publish newly produced/mutated references before that
    // callback; the previous allocation/call snapshot may no longer be live.
    // Disabled precise-root lowering generates no additional IR here.
    if(!snapshot_runtime_local_func_llvm_jit_gc_roots(state)) [[unlikely]] { return false; }
    // [validated expression bytes ...] | code_end
    // [safe                          ] | unsafe (one-past)
    // The scalar offset was captured before any immediate decoder advanced its
    // cursor. The shared emitter rechecks its half-open bounds; no source pointer
    // is advanced or dereferenced here.
    if(!emit_runtime_local_func_llvm_jit_debug_safe_point(state, state.current_wasm_op_offset)) [[unlikely]] { return false; }
    state.last_debug_instruction_offset = state.current_wasm_op_offset;
    return true;
}

// Initialize the full per-function emit state, create typed entry/core functions, allocate locals, and seed the control
// stack with the implicit function return label.
#include "single_func_pending_numeric_core.h"

#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
namespace sealed_compact_codegen {
    inline bool capture(runtime_local_func_llvm_jit_emit_state_t&) noexcept;
    inline bool retire(runtime_local_func_llvm_jit_emit_state_t&) noexcept;
}
#endif
#include "single_func_checkpoint_emit.h"

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
                                                                                       bool emit_unwind_call_stack_frames = false,
                                                                                       bool emit_debug_safe_points = false,
                                                                                       llvm_jit_compilation_mode compilation_mode = llvm_jit_compilation_mode::unspecified,
                                                                                       llvm_jit_debug_safe_point_granularity debug_safe_point_granularity = llvm_jit_debug_safe_point_granularity::entry_loop,
                                                                                       ::std::uintptr_t debug_full_patchable_typed_target_base_address = 0u,
                                                                                       ::std::size_t debug_full_patchable_typed_target_count = 0uz,
                                                                                       ::uwvm2::utils::container::vector<::std::uint_least8_t>* debug_safe_point_bits = nullptr,
                                                                                       bool emit_precise_gc_root_frames = false,
                                                                                       ::uwvm2::runtime::exception::pending_experiment::numeric_entry::compile_plan const* pending_numeric_plan = nullptr,
                                                                                       ::std::uint64_t debug_compiled_function_generation = 1u,
                                                                                       llvm_jit_debug_local_initialization_query debug_local_initialization = {},
                                                                                       ::uwvm2::runtime::checkpoint::function_plan* checkpoint_plan = nullptr) noexcept
{
    state = {};
    state.compilation_mode = compilation_mode;
    if(pending_numeric_plan != nullptr &&
       (!pending_numeric_plan->shape_valid_for_compilation() || pending_numeric_plan->module != local_func_storage.runtime_module_ptr ||
        pending_numeric_plan->module_id != local_func_storage.module_id ||
        compilation_mode != llvm_jit_compilation_mode::full ||
        route_wasm_calls_through_runtime_bridge || emit_tiered_loop_reentry_entries ||
        emit_debug_safe_points || debug_full_patchable_typed_target_base_address != 0u ||
        lazy_defined_raw_call_target_base_address != 0u || lazy_defined_typed_entry_target_base_address != 0u))
    { module_storage.discard_emission_preserving_first_decline(); return false; }
    // [host-pinned immutable compilation shape] borrowed only through this compile.
    // [safe                                  ] this private borrow grants no executable admission;
    // the runtime binds a separately validated admitted plan after every body succeeds.
    state.pending_numeric_plan = pending_numeric_plan;
    if(emit_debug_safe_points &&
       (debug_compiled_function_generation == 0u || compilation_mode != llvm_jit_compilation_mode::full ||
        (debug_safe_point_granularity != llvm_jit_debug_safe_point_granularity::entry_loop &&
         debug_safe_point_granularity != llvm_jit_debug_safe_point_granularity::instruction) || route_wasm_calls_through_runtime_bridge ||
        lazy_defined_raw_call_target_base_address != 0u || lazy_defined_raw_call_target_count != 0uz ||
        lazy_defined_typed_entry_target_base_address != 0u || lazy_defined_typed_entry_target_count != 0uz ||
        lazy_defined_targets_are_atomic || emit_tiered_loop_reentry_entries)) [[unlikely]]
    {
        // A rejected full-only instrumentation request must invalidate the
        // aggregate output too; finalization otherwise treats an empty module
        // with declarations as emitted successfully. No native body is published.
        module_storage.discard_emission_preserving_first_decline();
        return false;
    }
    bool const patchable{debug_full_patchable_typed_target_base_address != 0u || debug_full_patchable_typed_target_count != 0uz};
    if(patchable &&
       (!emit_debug_safe_points || compilation_mode != llvm_jit_compilation_mode::full ||
        local_func_storage.runtime_module_ptr == nullptr || debug_full_patchable_typed_target_base_address == 0u ||
        debug_full_patchable_typed_target_count != local_func_storage.runtime_module_ptr->local_defined_function_vec_storage.size() ||
        debug_full_patchable_typed_target_count == 0uz ||
        debug_full_patchable_typed_target_count > SIZE_MAX / sizeof(::std::uintptr_t) ||
        debug_full_patchable_typed_target_base_address % ::std::atomic_ref<::std::uintptr_t>::required_alignment != 0u ||
        sizeof(::std::uintptr_t) % ::std::atomic_ref<::std::uintptr_t>::required_alignment != 0uz ||
        debug_full_patchable_typed_target_base_address > UINTPTR_MAX - debug_full_patchable_typed_target_count * sizeof(::std::uintptr_t))) [[unlikely]]
    {
        // No host table is dereferenced and no external symbol is published on rejection.
        module_storage.discard_emission_preserving_first_decline();
        return false;
    }
    state.debug_full_patchable_typed_target_base_address = debug_full_patchable_typed_target_base_address;
    state.debug_full_patchable_typed_target_count = debug_full_patchable_typed_target_count;
    state.emit_debug_safe_points = emit_debug_safe_points;
    state.debug_compiled_function_generation = debug_compiled_function_generation;
    // [caller-owned bitmap bytes ...] bitmap_end
    // [safe                         ] unsafe (one-past)
    //  ^^ the borrowed owner lives through validation; no pointer is advanced.
    state.debug_safe_point_bits = debug_safe_point_bits;
    state.debug_safe_point_granularity = debug_safe_point_granularity;
    // Compiler-only context, never emitted as a native/guest address. The same
    // fused call owns it through all loop/else/end and instruction points.
    state.debug_local_initialization = debug_local_initialization;
    state.checkpoint_plan = checkpoint_plan;
    state.verify_llvm_jit_ir = verify_llvm_jit_ir || checkpoint_plan != nullptr; // selected recording cannot skip verifier
    state.route_wasm_calls_through_runtime_bridge = route_wasm_calls_through_runtime_bridge;
    state.lazy_defined_raw_call_target_base_address = lazy_defined_raw_call_target_base_address;
    state.lazy_defined_raw_call_target_count = lazy_defined_raw_call_target_count;
    state.lazy_defined_typed_entry_target_base_address = lazy_defined_typed_entry_target_base_address;
    state.lazy_defined_typed_entry_target_count = lazy_defined_typed_entry_target_count;
    state.lazy_defined_targets_are_atomic = lazy_defined_targets_are_atomic;
    state.emit_tiered_loop_reentry_entries = emit_tiered_loop_reentry_entries;
    state.emit_call_stack_frames = emit_call_stack_frames;
    state.emit_unwind_call_stack_frames = emit_unwind_call_stack_frames;
    state.emit_precise_gc_root_frames = emit_precise_gc_root_frames;
    // [LLVM module-owned declarations] copy borrowed handles only; the owning
    // [safe                         ] module/context outlive this emit state.
    state.native_exception_imports = module_storage.native_exception_imports;
    // Imported calls may propagate an exception even when this module's Wasm
    // syntax policy disables exception opcodes. CFI and cold frame cleanup are
    // part of the VM call ABI; opcode acceptance remains the validator's policy.
    state.native_guest_exceptions = state.native_exception_imports.status == ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::error::ok;
    if(state.native_guest_exceptions)
    {
        state.native_exception_runtime = ::uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad::declare_catch_runtime(
            *module_storage.llvm_module);
        if(!state.native_exception_runtime) { return false; }
    }
#ifdef UWVM_CPP_EXCEPTIONS
    // Without qualified native propagation we cannot retire frame roots on
    // every exceptional exit. A partial mapping must never enable collection.
    if(emit_precise_gc_root_frames && !state.native_guest_exceptions) { return false; }
#endif
    // Exception-enabled builds need actual qualified frame-exit cleanup. Keep
    // existing debug safe points available when native EH is unqualified, but
    // refuse activation identity instead of retaining a dead frame. Normal full
    // never enables this private producer. No-C++-EH cannot unwind this frame.
    state.debug_activation_enabled = emit_debug_safe_points;
#ifdef UWVM_CPP_EXCEPTIONS
    state.debug_activation_enabled = state.debug_activation_enabled && state.native_guest_exceptions;
#endif

    if(checkpoint_plan != nullptr && (!checkpoint_plan->profile || !state.debug_activation_enabled ||
       !emit_precise_gc_root_frames || compilation_mode != llvm_jit_compilation_mode::full ||
       pending_numeric_plan != nullptr)) { module_storage.discard_emission_preserving_first_decline(); return false; }

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
    if(pending_numeric_plan != nullptr)
    {
        if(!state.native_guest_exceptions) { return false; }
        auto const numeric{[](runtime_operand_stack_value_type type) noexcept
        { return type == runtime_operand_stack_value_type::i32 || type == runtime_operand_stack_value_type::i64 ||
                 type == runtime_operand_stack_value_type::f32 || type == runtime_operand_stack_value_type::f64 ||
                 type == runtime_operand_stack_value_type::v128; }};
        for(auto const type: state.local_types) { if(!numeric(type)) { return false; } }
        for(::std::size_t index{}; index != func_result_count_uz; ++index)
        { if(!numeric(static_cast<runtime_operand_stack_value_type>(func_result_begin[index]))) { return false; } }
    }

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
    else if(state.native_guest_exceptions) { state.llvm_public_entry_function->setUWTableKind(::llvm::UWTableKind::Sync); }
    if(emit_tiered_loop_reentry_entries)
    {
        // Tiered mode splits the function into a public typed wrapper and an internal core.  The core receives two hidden
        // arguments before the Wasm parameters: reentry id and serialized-local base address.
        auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
        ::uwvm2::utils::container::vector<::llvm::Type*> core_parameter_types{};
        core_parameter_types.reserve(llvm_parameter_types.size() + 2uz);
        core_parameter_types.push_back(::llvm::Type::getInt32Ty(llvm_context));
        core_parameter_types.push_back(llvm_intptr_type);
        for(auto param_type: llvm_function_type->params()) { core_parameter_types.push_back(param_type); }

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
        else if(state.native_guest_exceptions) { state.llvm_function->setUWTableKind(::llvm::UWTableKind::Sync); }

        state.tiered_core_entry_id_arg = state.llvm_function->getArg(0u);
        state.tiered_core_local_base_arg = state.llvm_function->getArg(1u);
        if(state.tiered_core_entry_id_arg == nullptr || state.tiered_core_local_base_arg == nullptr) [[unlikely]] { return false; }
    }
    else if(pending_numeric_plan != nullptr)
    {
        auto const core{pending_numeric_core_declaration(*state.llvm_module, llvm_context,
            *runtime_module_ptr, function_index, *function_type_ptr)};
        if(core == nullptr || !core->empty()) { return false; }
        // [same LLVM module][empty exact native-context core declaration]
        // [safe                                                        ]
        state.llvm_function = core;
        // [function-owned argument 0][context then unchanged public args]
        // [safe                                                       ]
        state.pending_numeric_context = core->getArg(0u);
        if(state.emit_unwind_call_stack_frames) { apply_llvm_jit_unwind_call_stack_function_attrs(*core); }
        else { core->setUWTableKind(::llvm::UWTableKind::Sync); }
    }
    else
    {
        state.llvm_function = state.llvm_public_entry_function;
    }

    if(state.debug_activation_enabled)
    {
        state.llvm_function->addFnAttr("uwvm.debug.activation");
#if defined(__linux__) && defined(UWVM_CPP_EXCEPTIONS) && LLVM_VERSION_MAJOR >= 23 && \
    (((defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4)) && \
        defined(UWVM2_ENABLE_DEBUG_NATIVE_CONTINUATION_LINUX_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_CONTINUATION_LINUX_PRODUCT == 1 || \
     ((defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)) && defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1))
        // Actual debug functions and typed wrappers need asynchronous CFI;
        // the diagnostic instruction/unwind policy remains independent.
        apply_llvm_jit_unwind_call_stack_function_attrs(*state.llvm_function);
        if(state.llvm_public_entry_function != state.llvm_function)
        { apply_llvm_jit_unwind_call_stack_function_attrs(*state.llvm_public_entry_function); }
# if (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips))
        // Keep short local edges within the true body. The backend still
        // expands out-of-range branches; section-tail stubs grant no authority.
        state.llvm_function->addFnAttr("uwvm.native.debug.pcrel");
        if(state.llvm_public_entry_function != state.llvm_function)
        { state.llvm_public_entry_function->addFnAttr("uwvm.native.debug.pcrel"); }
# endif
#endif
    }
    if(state.checkpoint_plan != nullptr) { state.llvm_function->addFnAttr("uwvm.checkpoint.typed-entry-v1"); }

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
    if(state.emit_debug_safe_points)
    {
        // Identity is supplied by the admitted full compiler, not a guest custom section.
        // LLVM immediately owns this synthetic filename; no borrowed UTF-8 pointer survives.
        auto const identity{::uwvm2::utils::container::u8concat_uwvm(u8"uwvm-m",
            ::fast_io::mnp::dec(local_func_storage.module_id), u8"-f", ::fast_io::mnp::dec(local_func_storage.function_index),
            u8"-g", ::fast_io::mnp::dec(state.debug_compiled_function_generation), u8".wasm-native-v1")};
        state.debug_native_provenance = ::uwvm2::runtime::compiler::llvm_jit::native_provenance::attach(
            *state.llvm_function, get_llvm_string_ref(identity));
        if(state.debug_native_provenance == nullptr) { return false; }
        ::uwvm2::runtime::compiler::llvm_jit::native_provenance::unknown(*state.ir_builder, state.debug_native_provenance);
    }
    if(state.emit_call_stack_frames && (!emit_tiered_loop_reentry_entries || state.llvm_function->getCallingConv() == ::llvm::CallingConv::Tail) &&
       !emit_runtime_local_func_llvm_jit_call_stack_push(*state.ir_builder, local_func_storage.module_id, local_func_storage.function_index)) [[unlikely]]
    {
        return false;
    }
    // A tailcc public wrapper tail-enters this normal-init block, which owns
    // the logical activation. OSR bypasses normal-init and inherits the existing
    // interpreter activation. Non-tailcc cores retain wrapper ownership.

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
            auto const core_arg_index{emit_tiered_loop_reentry_entries ? local_index + 2uz :
                (pending_numeric_plan != nullptr ? local_index + 1uz : local_index)};
            state.ir_builder->CreateStore(state.llvm_function->getArg(core_arg_index), local_pointer);
        }
        else
        {
            auto zero_constant{get_llvm_zero_constant_from_wasm_value_type(llvm_context, local_type)};
            if(zero_constant == nullptr) [[unlikely]] { return false; }
            state.ir_builder->CreateStore(zero_constant, local_pointer);
        }
    }

    if(!prepare_runtime_llvm_jit_gc_root_frame(*state.ir_builder, *state.llvm_function,
        state.gc_root_frame, state.emit_precise_gc_root_frames)) { return false; }
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
    if(!sealed_compact_codegen::capture(state)) { return false; }
#endif
    if(state.emit_debug_safe_points)
    {
        auto const count{(::std::min)(state.local_types.size(), ::uwvm2::runtime::lib::details::llvm_jit_debug_max_captured_locals)};
        if(count != 0u)
        {
            auto const slot{::llvm::ArrayType::get(state.ir_builder->getInt8Ty(),
                ::uwvm2::runtime::lib::details::llvm_jit_debug_local_slot_bytes)};
            state.debug_local_snapshot_type = ::llvm::ArrayType::get(slot, static_cast<unsigned>(count));
            auto const availability{::llvm::ArrayType::get(state.ir_builder->getInt8Ty(), static_cast<unsigned>(count))};
            // Both byte arrays have ABI alignment 1. LLVM struct field 0 starts
            // at zero, and field 1 follows exactly count*16 bytes, on every ABI.
            // Existing five-argument native/ASM bridge signature stays intact.
            state.debug_local_snapshot_packet_type = ::llvm::StructType::get(llvm_context,
                {state.debug_local_snapshot_type, availability}, false);
            state.debug_local_snapshot = create_llvm_jit_entry_block_alloca(*state.ir_builder,
                state.debug_local_snapshot_packet_type, nullptr, get_llvm_string_ref(u8"debug.locals.snapshot.available.v2"));
            if(state.debug_local_snapshot == nullptr) [[unlikely]] { return false; }
            // Every payload and availability byte is initialized. A false flag
            // never grants permission to interpret that slot's zero padding.
            state.ir_builder->CreateStore(::llvm::ConstantAggregateZero::get(state.debug_local_snapshot_packet_type),
                                          state.debug_local_snapshot);
        }
    }

    // Actual body init executes exactly once per physical typed entry; loop
    // offset zero and later backedges cannot mint another incarnation.
    if(!emit_runtime_local_func_llvm_jit_debug_activation_enter(state)) { return false; }
    if(!prepare_runtime_local_func_llvm_jit_checkpoint_observer_workspace(state)) { return false; }
    if(!emit_runtime_local_func_llvm_jit_checkpoint_initialization_flags(
        state, func_parameter_count_uz, state.checkpoint_executed_local_flags)) { return false; }
    if(!prepare_runtime_local_func_llvm_jit_checkpoint_resume_entry(state)) { return false; }
    // Selector zero alone captures ordinary initial locals. Restored
    // landings bypass this point and publish the saved original indices.
    if(!emit_runtime_local_func_llvm_jit_checkpoint_entry(state)) { return false; }

    state.self_tail_entry_block = ::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"body"), state.llvm_function);
    state.ir_builder->CreateBr(state.self_tail_entry_block);
    state.ir_builder->SetInsertPoint(state.self_tail_entry_block);
    if(!snapshot_runtime_local_func_llvm_jit_gc_roots(state)) { return false; }
    // A self-tail transfer reuses this body block after resetting its locals.
    // It therefore observes a new entry point without inserting instructions
    // between any genuine musttail call and its required immediate return.
    if(state.emit_debug_safe_points && state.debug_safe_point_granularity == llvm_jit_debug_safe_point_granularity::entry_loop &&
       !emit_runtime_local_func_llvm_jit_debug_safe_point(state, 0uz))
        [[unlikely]] { return false; }

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

// Wrapper ownership differs from the Wasm body: ordinary public wrappers own
// their logical activation; TailCC OSR wrappers borrow an interpreter activation
// that the core consumes. Only their exceptional edge needs the supplied repair.
template<typename ExceptionalCleanup>
[[nodiscard]] inline ::llvm::CallBase* emit_llvm_jit_wrapper_core_call(
    runtime_local_func_llvm_jit_emit_state_t const& state, ::llvm::IRBuilder<>& builder,
    ::llvm::Function* core, ::llvm::ArrayRef<::llvm::Value*> arguments,
    bool needs_cleanup, ExceptionalCleanup&& cleanup) noexcept
{
    if(!state.native_guest_exceptions || !needs_cleanup) { return builder.CreateCall(core, arguments); }
    auto const function{builder.GetInsertBlock()->getParent()};
    function->setPersonalityFn(state.native_exception_imports.personality);
    auto const origin{builder.saveIP()};
    // [wrapper-owned continuation/cleanup blocks] each insertion cursor below
    // [safe                                    ] borrows a complete live block.
    auto const normal{::llvm::BasicBlock::Create(builder.getContext(), "guest.wrapper.return", function)};
    auto const unwind{::llvm::BasicBlock::Create(builder.getContext(), "guest.wrapper.cleanup", function)};
    builder.SetInsertPoint(unwind);
    auto const record{builder.CreateLandingPad(::llvm::StructType::get(builder.getContext(),
        {builder.getPtrTy(), builder.getInt32Ty()}), 0u)};
    record->setCleanup(true);
    if(!cleanup(builder)) { return nullptr; }
    builder.CreateResume(record);
    // [original nonterminated wrapper block] its saved cursor is still live.
    builder.restoreIP(origin);
    auto const invoke{builder.CreateInvoke(core, normal, unwind, arguments)};
    // [normal continuation] results and normal ownership repair run only here.
    builder.SetInsertPoint(normal);
    return invoke;
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

    // Native metadata must not attribute synthetic cleanup/returns to the previous opcode.
    if(state.emit_debug_safe_points)
    { ::uwvm2::runtime::compiler::llvm_jit::native_provenance::unknown(ir_builder, state.debug_native_provenance); }

    // Seal the synthetic function return block.  If there are no incoming edges for a result-returning function, the body
    // was unreachable and the return block must become unreachable rather than returning an undef value.
    // Finalization runs only after the implicit function context has been closed by `end`, so no more branch incoming
    // edges can be added after this point.
    ir_builder.SetInsertPoint(state.return_block);
    if(!finalize_runtime_llvm_jit_gc_root_frame(state.gc_root_frame)) { return false; }
    if(state.func_result_count_uz == 0uz)
    {
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        if(!sealed_compact_codegen::retire(state)) { return false; }
#endif
        if(!emit_runtime_local_func_llvm_jit_debug_activation_leave(ir_builder, state, 0u)) { return false; }
        if(!leave_runtime_llvm_jit_gc_root_frame(ir_builder, state.gc_root_frame)) { return false; }
        if(state.emit_call_stack_frames && (!state.emit_tiered_loop_reentry_entries || state.llvm_function->getCallingConv() == ::llvm::CallingConv::Tail) &&
           !emit_runtime_local_func_llvm_jit_call_stack_pop(ir_builder)) [[unlikely]]
        {
            return false;
        }
        ir_builder.CreateRetVoid();
    }
    else if(state.return_phis.size() == state.func_result_count_uz && !state.return_phis.empty() &&
            state.return_phis.index_unchecked(0uz)->getNumIncomingValues() != 0u)
    {
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        if(!sealed_compact_codegen::retire(state)) { return false; }
#endif
        if(!emit_runtime_local_func_llvm_jit_debug_activation_leave(ir_builder, state, 0u)) { return false; }
        if(!leave_runtime_llvm_jit_gc_root_frame(ir_builder, state.gc_root_frame)) { return false; }
        if(state.emit_call_stack_frames && (!state.emit_tiered_loop_reentry_entries || state.llvm_function->getCallingConv() == ::llvm::CallingConv::Tail) &&
           !emit_runtime_local_func_llvm_jit_call_stack_pop(ir_builder)) [[unlikely]]
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
        if(packed_return == nullptr) [[unlikely]] { return false; }
        if(state.func_result_count_uz > 1uz)
        {
            // [surviving caller's result buffer ...] its address is the final ABI
            // [safe                               ] argument, not an alloca of this frame.
            auto const result_address{state.llvm_function->getArg(state.llvm_function->arg_size() - 1uz)};
            if(!emit_store_runtime_wasm_call_result_to_raw_buffer(ir_builder, *state.local_func_storage_ptr->function_type_ptr,
                packed_return, result_address, get_llvm_string_ref(u8"return.tuple"))) { return false; }
            ir_builder.CreateRetVoid();
        }
        else
        {
            if(packed_return->getType() != state.llvm_function->getReturnType()) { return false; }
            ir_builder.CreateRet(packed_return);
        }
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
                if(state.emit_precise_gc_root_frames)
                {
                    ::std::size_t count{};
                    for(::std::size_t index{}; index != state.local_types.size(); ++index)
                    {
                        if(!llvm_jit_gc_root_carrier_type(state.local_types[index])) { continue; }
                        auto const pointer{state.local_pointers.index_unchecked(index)};
                        auto const value{load_builder.CreateLoad(pointer->getAllocatedType(), pointer, "gc.osr.root")};
                        if(!emit_runtime_llvm_jit_gc_root_slot(load_builder, state.gc_root_frame, value, count++)) { return false; }
                    }
                    if(!publish_runtime_llvm_jit_gc_root_count(load_builder, state.gc_root_frame, count)) { return false; }
                }
                load_builder.CreateBr(target_block);
            }

            return true;
        }};

    auto const emit_runtime_local_func_llvm_jit_tiered_public_entry_wrapper{
        [&]() constexpr noexcept -> bool
        {
            // Tailcc wrappers must retire before entering the core; retaining a
            // wrapper on every cross-function tail edge would grow native stacks.
            // The core owns logical push/pop in this ABI. Other ABIs keep the
            // existing wrapper-owned activation until their tail ABI is ready.
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

            bool const tail_entry{public_function->getCallingConv() == ::llvm::CallingConv::Tail};
            if(state.emit_call_stack_frames && !tail_entry &&
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

            ::llvm::CallBase* core_call{};
            if(tail_entry)
            {
                // A tail transfer retires this wrapper and its handlers. Keep the
                // musttail-only path a CallInst immediately followed by return.
                auto const tail_call{apply_llvm_jit_wasm_calling_conv(public_builder.CreateCall(
                    core_function, {core_arguments.data(), core_arguments.size()}))};
                tail_call->setTailCallKind(::llvm::CallInst::TCK_MustTail);
                core_call = tail_call; // [safe] function-owned instruction, same live wrapper.
            }
            else
            {
                core_call = apply_llvm_jit_wasm_calling_conv(emit_llvm_jit_wrapper_core_call(state, public_builder,
                    core_function, {core_arguments.data(), core_arguments.size()}, state.emit_call_stack_frames,
                    [](::llvm::IRBuilder<>& cold) noexcept { return emit_runtime_local_func_llvm_jit_call_stack_pop(cold); }));
                // [safe] either live wrapper call/invoke, or rejected before use.
                if(core_call == nullptr) { return false; }
            }
            if(state.emit_call_stack_frames && !tail_entry && !emit_runtime_local_func_llvm_jit_call_stack_pop(public_builder)) [[unlikely]] { return false; }

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
                else if(state.native_guest_exceptions) { reentry_function->setUWTableKind(::llvm::UWTableKind::Sync); }

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

                if(abi_layout.result_count > 1uz) { core_arguments.push_back(result_buffer_address); }
                bool const restore_inherited_frame{state.emit_call_stack_frames && core_function->getCallingConv() == ::llvm::CallingConv::Tail};
                auto core_call{apply_llvm_jit_wasm_calling_conv(emit_llvm_jit_wrapper_core_call(state, osr_builder,
                    core_function, {core_arguments.data(), core_arguments.size()}, restore_inherited_frame,
                    [&](::llvm::IRBuilder<>& cold) noexcept
                    {
                        // The core has already popped the inherited interpreter
                        // activation while unwinding. Restore exactly that entry
                        // before its suspended C++ owner performs the final pop.
                        return emit_runtime_local_func_llvm_jit_call_stack_push(cold,
                            local_func_storage_ptr->module_id, local_func_storage_ptr->function_index);
                    }))};
                if(core_call == nullptr) { return false; }
                if(abi_layout.result_count == 1uz)
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

                // The tailcc core consumed the inherited interpreter activation,
                // either by returning or by transferring it to a tail target. The
                // suspended interpreter's owner still expects to pop that frame
                // when this wrapper returns, so restore it only after the native
                // chain completes. It is absent during every native tail target.
                if(state.emit_call_stack_frames && core_function->getCallingConv() == ::llvm::CallingConv::Tail &&
                   !emit_runtime_local_func_llvm_jit_call_stack_push(osr_builder,
                       local_func_storage_ptr->module_id, local_func_storage_ptr->function_index)) { return false; }
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
            else if(state.native_guest_exceptions) { raw_entry_function->setUWTableKind(::llvm::UWTableKind::Sync); }
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

            if(abi_layout.result_count > 1uz) { call_arguments.push_back(result_buffer_address); }
            auto typed_call{apply_llvm_jit_wasm_calling_conv(raw_ir_builder.CreateCall(llvm_function, {call_arguments.data(), call_arguments.size()}))};
            if(abi_layout.result_count == 1uz)
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

    if(state.pending_numeric_plan != nullptr && !emit_pending_numeric_public_wrapper(state)) { return false; }
    if(!emit_runtime_local_func_llvm_jit_raw_entry_wrapper()) [[unlikely]] { return false; }

    if(!finalize_runtime_llvm_jit_gc_root_frame(state.gc_root_frame, true)) { return false; }
    if(!state.emit_debug_safe_points && state.debug_native_provenance == nullptr && !state.emit_call_stack_frames)
    { coalesce_runtime_llvm_jit_generic_trap_blocks(*state.llvm_function); }
    if(!verify_llvm_jit_function(*state.llvm_function, state.verify_llvm_jit_ir)) [[unlikely]] { return false; }
    if(state.debug_native_provenance != nullptr)
    {
        ::uwvm2::runtime::compiler::llvm_jit::native_provenance::restrict_public_code(
            *state.llvm_function, state.debug_native_numeric_identities);
        if(!verify_llvm_jit_function(*state.llvm_function, state.verify_llvm_jit_ir)) { return false; }
    }
    if(!finalize_runtime_local_func_llvm_jit_checkpoint_resume_entry(state)) { return false; }
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
    if(!add_runtime_local_func_llvm_jit_checkpoint_control_prefix_incoming(state,target,predecessor)) { return false; }
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

// Core 3 throw_ref on a statically null exn/noexn always traps. The adjacent
// ref.null lowered no SSA carrier; this cold terminal path adds no work to
// ordinary calls, memory accesses, or numeric exception throws.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_null_throw_ref(
    runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(!state.valid || state.llvm_module == nullptr || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]]
    { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    auto& ir_builder{*state.ir_builder};
    auto const current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || llvm_jit_basic_block_has_terminator(current_block)) [[unlikely]] { return false; }
    emit_llvm_runtime_trap(ir_builder, ::uwvm2::runtime::lib::llvm_jit_trap_kind::null_reference);
    ir_builder.CreateUnreachable();
    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

#include "single_func_exception_emit.h"
#include "single_func_checkpoint_nested_child_emit.h"
#include "single_func_checkpoint_retirement_emit.h"
#include "single_func_debug_shutdown_emit.h"

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

    llvm_jit_checkpoint_control_prefix_t checkpoint_prefix{};
    if(!prepare_runtime_local_func_llvm_jit_checkpoint_control_prefix(state,outer_stack_size,end_block,
        nullptr,nullptr,checkpoint_prefix)) { return false; }
    auto const control_stack_index{state.control_stack.size()};
    state.control_stack.push_back({.type = llvm_jit_control_context_type::block,
                                   .params = block_signature.params,
                                   .result = block_signature.results,
                                   .end_block = end_block,
                                   .end_phis = end_phis,
                                   .else_block = nullptr,
                                   .outer_stack_size = outer_stack_size,
                                   .entry_params = {},
                                   .checkpoint_prefix = checkpoint_prefix,
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

    llvm_jit_checkpoint_control_prefix_t checkpoint_prefix{};
    if(!prepare_runtime_local_func_llvm_jit_checkpoint_control_prefix(state,outer_stack_size,end_block,
        loop_body_block,current_block,checkpoint_prefix)) { return false; }
    ir_builder.CreateBr(loop_body_block);
    ir_builder.SetInsertPoint(loop_body_block);
    truncate_runtime_local_func_llvm_jit_operand_stack_to(state, outer_stack_size);
    if(!use_runtime_local_func_llvm_jit_checkpoint_control_prefix(state,checkpoint_prefix,false,true)) { return false; }
    for(::std::size_t param_index{}; param_index != loop_param_phis.size(); ++param_index)
    {
        state.operand_stack.push_back({.type = block_signature.params.begin[param_index],
                                       .value = loop_param_phis.index_unchecked(param_index)});
    }
    // [operand prefix][this iteration's loop parameter PHIs] end
    // [safe                                                ] publish only after
    // replacing entry descriptors with the actual backedge/OSR PHI values.
    if(!snapshot_runtime_local_func_llvm_jit_gc_roots(state)) { return false; }
    // GC roots precede any debug callback that might park or reenter. Disabled
    // root lowering adds no IR; ordinary debug loop code remains identical.
    if(state.emit_debug_safe_points && state.checkpoint_observer_controls == nullptr)
    {
        if(state.debug_safe_point_granularity == llvm_jit_debug_safe_point_granularity::instruction)
        {
            if(!emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) [[unlikely]] { return false; }
        }
        else if(!emit_runtime_local_func_llvm_jit_debug_safe_point(state, state.current_wasm_op_offset))
            [[unlikely]] { return false; }
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
                                   .checkpoint_prefix = checkpoint_prefix,
                                   .outer_branch_target_stack_size = state.branch_target_stack.size(),
                                   .is_reachable = true,
                                   .end_block_has_incoming = false});
    state.branch_target_stack.push_back(
        {.params = block_signature.params, .block = loop_body_block, .phis = loop_param_phis, .control_stack_index = control_stack_index});
    if(state.checkpoint_observer_controls != nullptr &&
       !emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) { return false; }
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

    ::std::size_t saved_parameter_first{};
    if(!prepare_runtime_local_func_llvm_jit_checkpoint_saved_if(state,entry_params,saved_parameter_first)) { return false; }
    auto cond_i1{ir_builder.CreateICmpNE(condition.value, ::llvm::ConstantInt::get(condition.value->getType(), 0u))};
    ir_builder.CreateCondBr(cond_i1, then_block, else_block);
    ir_builder.SetInsertPoint(then_block);

    llvm_jit_checkpoint_control_prefix_t checkpoint_prefix{};
    if(!prepare_runtime_local_func_llvm_jit_checkpoint_control_prefix(state,outer_stack_size,end_block,
        nullptr,nullptr,checkpoint_prefix)) { return false; }
    auto const control_stack_index{state.control_stack.size()};
    state.control_stack.push_back({.type = llvm_jit_control_context_type::if_then,
                                   .params = block_signature.params,
                                   .result = block_signature.results,
                                   .end_block = end_block,
                                   .end_phis = end_phis,
                                   .else_block = else_block,
                                   .outer_stack_size = outer_stack_size,
                                   .entry_params = entry_params,
                                   .checkpoint_saved_parameter_first = saved_parameter_first,
                                   .checkpoint_prefix = checkpoint_prefix,
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
    state.ir_builder->SetInsertPoint(current_context.else_block);
    if(!use_runtime_local_func_llvm_jit_checkpoint_control_prefix(state,current_context.checkpoint_prefix,true)) { return false; }
    for(::std::size_t i{};i != current_context.entry_params.size();++i)
    {
        auto value{current_context.entry_params.index_unchecked(i)};
        value.value=read_runtime_local_func_llvm_jit_checkpoint_saved_if(state,*state.ir_builder,current_context,i);
        if(value.value == nullptr) { return false; }
        invalidate_runtime_local_func_llvm_jit_checkpoint_value_witness(state,value);
        state.operand_stack.push_back(value);
    }
    current_context.type = llvm_jit_control_context_type::if_else;
    current_context.is_reachable = true;
    // Only the false arm reaches the lexical else point. The then arm already
    // branches to its merge, so this call cannot be attached to that predecessor.
    return emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state);
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

    // The implicit function end executes only on lexical fallthrough. An early
    // return or a musttail transfer must not acquire an extra synthetic stop in
    // the shared return block after its terminating source instruction.
    if(current_context.type == llvm_jit_control_context_type::function && current_context.is_reachable &&
       !emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) [[unlikely]] { return false; }

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

        ::llvm::IRBuilder<> else_builder(current_context.else_block);
        for(::std::size_t result_index{}; result_index != result_count; ++result_index)
        {
            auto false_result{current_context.entry_params.index_unchecked(result_index)};
            false_result.value=read_runtime_local_func_llvm_jit_checkpoint_saved_if(state,else_builder,current_context,result_index);
            auto false_result_phi{current_context.end_phis.index_unchecked(result_index)};
            if(false_result.value == nullptr || false_result.type != current_context.result.begin[result_index] || false_result_phi == nullptr ||
               false_result_phi->getType() != false_result.value->getType()) [[unlikely]]
            {
                return false;
            }
            false_result_phi->addIncoming(false_result.value, current_context.else_block);
        }
        auto const& prefix{current_context.checkpoint_prefix};
        if(prefix.end_phis.size() != prefix.entry.size()) { return false; }
        for(::std::size_t i{};i != prefix.entry.size();++i)
        { prefix.end_phis.index_unchecked(i)->addIncoming(prefix.entry.index_unchecked(i).value,current_context.else_block); }
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
    auto const checkpoint_prefix{current_context.checkpoint_prefix};

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
            for(auto phi : checkpoint_prefix.end_phis)
            { if(phi != nullptr && phi->getNumIncomingValues() == 0u) { phi->eraseFromParent(); } }
            ::llvm::IRBuilder<> unreachable_builder(end_block);
            unreachable_builder.CreateUnreachable();
        }
        return true;
    }
    if(end_block == nullptr) [[unlikely]] { return false; }

    state.ir_builder->SetInsertPoint(end_block);
    if(!use_runtime_local_func_llvm_jit_checkpoint_control_prefix(state,checkpoint_prefix,false)) { return false; }
    auto const result_count{get_runtime_block_result_count(block_result)};
    if(end_phis.size() != result_count) [[unlikely]] { return false; }
    for(::std::size_t result_index{}; result_index != result_count; ++result_index)
    {
        auto phi{end_phis.index_unchecked(result_index)};
        if(phi == nullptr || phi->getNumIncomingValues() == 0u) [[unlikely]] { return false; }
        state.operand_stack.push_back({.type = block_result.begin[result_index], .value = phi});
    }
    // Forward branches and exception handlers can enter this merge without
    // executing its lexical predecessor. Emit after PHIs so all live incoming
    // paths report the same checked structural-end source position.
    return emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state);
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

// Core 3 br_on_null removes the reference on its taken edge; br_on_non_null removes
// it on fallthrough. PHIs receive the original reference SSA value without a payload copy.
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_br_on_reference(runtime_local_func_llvm_jit_emit_state_t& state,
    validation_module_traits_t::wasm_u32 label_index, bool non_null) noexcept
{
    if(!state.valid || state.llvm_function == nullptr || state.ir_builder == nullptr || state.control_stack.empty()) { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) { return false; }
    auto const reference{state.operand_stack.back()};
    if(reference.value == nullptr || !::uwvm2::validation::standard::wasm3::is_legacy_reference_carrier(reference.type)) { return false; }
    auto const target{get_runtime_local_func_llvm_jit_branch_target_by_depth(state, label_index)};
    if(target == nullptr) { return false; }
    auto& builder{*state.ir_builder};
    auto const current{builder.GetInsertBlock()};
    if(current == nullptr || llvm_jit_basic_block_has_terminator(current)) { return false; }
    auto const module{state.llvm_function->getParent()}; // LLVM-owned stable parent, never a guest address.
    auto const null_value{emit_llvm_jit_ref_is_null(builder, reference.value, module->getDataLayout().isLittleEndian())};
    if(null_value == nullptr) { return false; }
    if(!non_null) { state.operand_stack.pop_back(); }
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> values{};
    if(!try_get_runtime_local_func_llvm_jit_branch_values(state, target->params, values) ||
       !try_add_runtime_local_func_llvm_jit_branch_target_incoming(state, *target, values, current)) { return false; }
    auto const continuation{::llvm::BasicBlock::Create(state.llvm_function->getContext(), get_llvm_string_ref(u8"br_on_ref.cont"), state.llvm_function)};
    auto const condition{builder.CreateICmpEQ(null_value, builder.getInt32(non_null ? 0u : 1u))};
    builder.CreateCondBr(condition, target->block, continuation);
    // Builder cursor now names the newly allocated LLVM block, owned by this function.
    builder.SetInsertPoint(continuation);
    if(non_null) { state.operand_stack.pop_back(); }
    else { state.operand_stack.push_back(reference); }
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
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type,
    ::llvm::ArrayRef<llvm_jit_stack_value_t> extra_roots = {}) noexcept
{
    llvm_jit_prepared_wasm_call_operands_t prepared{};
    prepared.abi_layout = get_runtime_wasm_call_abi_layout(wasm_function_type);
    if(!prepared.abi_layout.valid || state.operand_stack.size() < prepared.abi_layout.parameter_count) [[unlikely]] { return prepared; }
    // [current locals][operand prefix][all validated call arguments] stack end
    // [safe                                                     ] snapshot
    // BEFORE pop removes typed descriptors. A later bare LLVM argument list
    // cannot distinguish numeric bits from reference carriers.
    if(!snapshot_runtime_local_func_llvm_jit_gc_roots(state, extra_roots)) { return prepared; }

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

// Resolve a validated same-module local call through a host-owned debug-full slot.
// This is a typed call, not a raw bridge: normal results/EH and musttail retain
// their existing ABI. Ordinary full compilation never invokes this helper.
[[nodiscard]] inline constexpr ::llvm::Value* emit_runtime_local_func_llvm_jit_debug_full_typed_target(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module,
    validation_module_traits_t::wasm_u32 function_index,
    ::llvm::FunctionType* function_type) noexcept
{
    if(state.local_func_storage_ptr == nullptr || state.local_func_storage_ptr->runtime_module_ptr != ::std::addressof(module) ||
       state.ir_builder == nullptr || function_type == nullptr || state.debug_full_patchable_typed_target_base_address == 0u) { return nullptr; }
    auto const imports{module.imported_function_vec_storage.size()};
    if(function_index < imports) { return nullptr; }
    auto const local_index{static_cast<::std::size_t>(function_index) - imports};
    if(local_index >= state.debug_full_patchable_typed_target_count || local_index >= module.local_defined_function_vec_storage.size()) { return nullptr; }
    // [validated module-owned signature] borrowed from the bounded local-function element.
    // [safe                            ] equal LLVM FunctionType includes every parameter and the explicit multi-result pointer.
    auto const signature{module.local_defined_function_vec_storage.index_unchecked(local_index).function_type_ptr};
    if(signature == nullptr || get_llvm_function_type_from_wasm_function_type(state.ir_builder->getContext(), *signature) != function_type)
    { return nullptr; }
    auto& builder{*state.ir_builder};
    auto const integer{::llvm::Type::getIntNTy(builder.getContext(), static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const table_type{::llvm::ArrayType::get(integer, state.debug_full_patchable_typed_target_count)};
    auto const name{::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(module), u8"_debug_full_typed_targets")};
    // [host-owned exact array extent ...] end
    // [safe                            ] preparation checked alignment/count/address overflow;
    //  ^^ base describes the entire stable allocation, not a scalar used as an array.
    auto const base{get_llvm_external_host_object_pointer(builder, state.debug_full_patchable_typed_target_base_address,
        table_type, ::uwvm2::utils::container::u8string_view{name.data(), name.size()})};
    if(base == nullptr) { return nullptr; }
    // [slot 0 ... slot local_index ... slot count-1] end
    // [safe                                       ] local_index < count above; never one-past.
    //            ^^ slot advances only within the exact LLVM array extent.
    auto const slot{builder.CreateInBoundsGEP(table_type, base,
        {::llvm::ConstantInt::get(integer, 0u), ::llvm::ConstantInt::get(integer, local_index)},
        get_llvm_string_ref(u8"call.debug.full.target.slot"))};
    auto const address{builder.CreateLoad(integer, slot, get_llvm_string_ref(u8"call.debug.full.target.address"))};
    address->setAlignment(::llvm::Align{::std::atomic_ref<::std::uintptr_t>::required_alignment});
    address->setAtomic(::llvm::AtomicOrdering::Acquire);
    address->setVolatile(true);
    // [published executable target] has the original complete typed ABI.
    // [safe                       ] host publication pins both new and retired code;
    //  ^^ callee is an executable pointer from that slot, never from guest memory.
    return builder.CreateIntToPtr(address, get_llvm_pointer_type(function_type), get_llvm_string_ref(u8"call.debug.full.target"));
}

// Emit a direct typed Wasm-to-Wasm call through an LLVM function declaration in the current module.
[[nodiscard]] inline constexpr ::llvm::Value*
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

    if(state.debug_full_patchable_typed_target_base_address != 0u)
    {
        auto const type{get_llvm_function_type_from_wasm_function_type(llvm_context, wasm_function_type)};
        // [validated local function signature] exactly determines the slot's callable ABI.
        // [safe                             ] target helper bounds the host-owned slot.
        auto const callee{emit_runtime_local_func_llvm_jit_debug_full_typed_target(state, runtime_module, func_index, type)};
        if(callee == nullptr) { return nullptr; }
        return emit_llvm_jit_typed_wasm_call(ir_builder, wasm_function_type, type, callee, call_arguments,
            [&](::llvm::FunctionType* call_type, ::llvm::Value* target, ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept
            { return emit_runtime_local_func_llvm_jit_may_throw_call(state, call_type, target, arguments); });
    }

    if(state.pending_numeric_plan != nullptr)
    {
        if(state.pending_numeric_context == nullptr ||
           state.pending_numeric_plan->module != ::std::addressof(runtime_module)) { return nullptr; }
        auto const core{pending_numeric_core_declaration(*llvm_module, llvm_context, runtime_module, func_index, wasm_function_type)};
        if(core == nullptr) { return nullptr; }
        return emit_llvm_jit_typed_wasm_call(ir_builder, wasm_function_type, core->getFunctionType(), core, call_arguments,
            [&](::llvm::FunctionType* type, ::llvm::Value* callee, ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept -> ::llvm::CallBase*
            {
                // [exact core ABI: hidden context + complete original operands]
                // [safe                                                       ]
                // Count is LLVM's checked unsigned extent (including the LAST
                // tuple buffer), so adding FIRST cannot wrap the native vector.
                if(type == nullptr || type->getNumParams() == 0u ||
                   arguments.size() != static_cast<::std::size_t>(type->getNumParams()) - 1uz)
                { return nullptr; }
                ::uwvm2::utils::container::vector<::llvm::Value*> actual{};
                actual.reserve(static_cast<::std::size_t>(type->getNumParams()));
                actual.push_back(state.pending_numeric_context);
                for(auto const argument: arguments) { actual.push_back(argument); }
                auto const called{emit_runtime_local_func_llvm_jit_may_throw_call(state, type, callee,
                    {actual.data(), actual.size()}, false)};
                if(called == nullptr) { return nullptr; }
                record_pending_numeric_call(*called, func_index, pending_numeric_call_can_escape_guest(state));
                if(!emit_pending_numeric_route(state, func_index)) { return nullptr; }
                // Multi-result loads happen only AFTER the empty-pending edge;
                // the ordinary tuple helper follows this callback in that block.
                return called;
            });
    }
    auto callee_function{get_or_create_llvm_wasm_function_declaration(*llvm_module, llvm_context, runtime_module, func_index, wasm_function_type)};
    if(callee_function == nullptr) [[unlikely]] { return nullptr; }
    return emit_llvm_jit_typed_wasm_call(ir_builder, wasm_function_type, callee_function->getFunctionType(), callee_function, call_arguments,
        [&](::llvm::FunctionType* type, ::llvm::Value* callee, ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
        {
            auto* const called{emit_runtime_local_func_llvm_jit_may_throw_call(state, type, callee, arguments)};
            if(state.native_eh_leaf_work)
            {
                // [actual current typed declaration/call/caller] live LLVM fragment.
                // [safe] Same-operation borrow; ABI check is deferred until the
                // outer typed-call helper has applied its calling convention.
                state.native_eh_leaf_work->fragment_call(called, callee_function, state.llvm_function, runtime_module, func_index);
            }
            return called;
        });
#else
        { return emit_runtime_local_func_llvm_jit_may_throw_call(state, type, callee, arguments); });
#endif
}

// Emit a call to a C++ runtime bridge function using the host calling convention.
template <auto BridgeFunction, bool MayThrowWasm = false>
[[nodiscard]] inline auto emit_runtime_local_func_llvm_jit_runtime_bridge_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                                      ::llvm::FunctionType* bridge_function_type,
                                                                                                      ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept -> ::std::conditional_t<MayThrowWasm, ::llvm::CallBase, ::llvm::CallInst>*
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.ir_builder == nullptr || bridge_function_type == nullptr) [[unlikely]] { return nullptr; }

    auto& ir_builder{*state.ir_builder};

    auto bridge_pointer{get_llvm_runtime_bridge_function_symbol_value<BridgeFunction>(ir_builder, bridge_function_type)};
    if(bridge_pointer == nullptr) [[unlikely]] { return nullptr; }
    if constexpr(MayThrowWasm)
    { return apply_llvm_jit_host_calling_conv(emit_runtime_local_func_llvm_jit_may_throw_call(state, bridge_function_type, bridge_pointer, arguments)); }
    else { return apply_llvm_jit_host_calling_conv(ir_builder.CreateCall(bridge_function_type, bridge_pointer, arguments)); }
}

// The retained profile is a compile-time engine selection. Observation-only
// debugger engines keep their existing bridges; ordinary memory/access IR gains
// no runtime check or probe. An invalid selected owner rejects compilation.
[[nodiscard]] inline bool runtime_local_func_llvm_jit_uses_checkpoint_effects(
    runtime_local_func_llvm_jit_emit_state_t const& state) noexcept
{
    return state.checkpoint_plan != nullptr && state.checkpoint_plan->profile &&
        state.checkpoint_plan->profile->purpose() == ::uwvm2::runtime::checkpoint::compilation_purpose::resumable;
}

// Dynamic raw entries require their original qualified raw convention. The
// debug-only runtime wrapper preserves it internally; LLVM calls the wrapper
// using its distinct host convention. Ordinary raw-call IR stays unchanged.
[[nodiscard]] inline ::llvm::CallBase* emit_runtime_local_func_llvm_jit_debug_raw_target_call(
    runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::Value* entry,
    ::llvm::ArrayRef<::llvm::Value*> raw_arguments) noexcept
{
    if(!state.debug_activation_enabled || state.ir_builder == nullptr || entry == nullptr || raw_arguments.size() != 5u) { return nullptr; }
    auto& builder{*state.ir_builder};
    auto const integer{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const type{get_llvm_runtime_raw_call_bridge_function_type(builder.getContext())};
    auto const bridge{runtime_local_func_llvm_jit_uses_checkpoint_effects(state) ?
        get_llvm_runtime_bridge_function_symbol_value<
            ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_raw_target_abi_bridge>(builder, type) :
        get_llvm_runtime_bridge_function_symbol_value<
            ::uwvm2::runtime::lib::details::llvm_jit_debug_raw_target_abi_bridge>(builder, type)};
    if(bridge == nullptr || entry->getType() != integer) { return nullptr; }
    ::uwvm2::utils::container::vector<::llvm::Value*> arguments{entry};
    for(auto const argument : raw_arguments)
    {
        if(argument == nullptr || argument->getType() != integer) { return nullptr; }
        arguments.push_back(argument); // owned LLVM handles, no generated/native pointer borrow escapes.
    }
    return apply_llvm_jit_host_calling_conv(emit_runtime_local_func_llvm_jit_may_throw_call(
        state, type, bridge, {arguments.data(), arguments.size()}));
}

// Shared raw-call helper: pack typed arguments into byte buffers, ask the caller to emit the bridge call, then load the
// typed result back from the result buffer if present. The callback may return any CallBase subclass; it must leave the
// builder at the normal-return continuation before this helper emits result loads. Existing callbacks create CallInst.
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
    auto const abi_layout{prepared_call.abi_layout};

    return emit_runtime_local_func_llvm_jit_runtime_raw_host_bridge_call(
        state,
        wasm_function_type,
        {prepared_call.arguments.data(), prepared_call.arguments.size()},
        param_buffer_name,
        result_buffer_name,
        [&](llvm_jit_runtime_raw_call_buffers_t const& raw_call_buffers) constexpr noexcept -> ::llvm::CallBase*
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

            // Generated code is already covered by the outer bridge token, LLVM-Wasm FP scope, and native-unwind execution gate.
            // Calling the public host/re-entry API here would redundantly reset FP state and recursively lock the gate
            // for every imported call.
            if(runtime_local_func_llvm_jit_uses_checkpoint_effects(state))
            {
                return emit_runtime_local_func_llvm_jit_runtime_bridge_call<
                    ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_call_raw_from_generated_wasm_abi_bridge, true>(
                    state, bridge_function_type,
                    {module_address, ::llvm::ConstantInt::get(llvm_intptr_type, func_index),
                     raw_call_buffers.result_buffer_address, ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.result_bytes),
                     raw_call_buffers.param_buffer_address, ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.parameter_bytes)});
            }
            if(state.checkpoint_plan != nullptr && state.checkpoint_plan->profile)
            {
                return emit_runtime_local_func_llvm_jit_runtime_bridge_call<
                    ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_observe_call_raw_from_generated_wasm_abi_bridge, true>(
                    state, bridge_function_type,
                    {module_address, ::llvm::ConstantInt::get(llvm_intptr_type, func_index),
                     raw_call_buffers.result_buffer_address, ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.result_bytes),
                     raw_call_buffers.param_buffer_address, ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.parameter_bytes)});
            }
            return emit_runtime_local_func_llvm_jit_runtime_bridge_call<::uwvm2::runtime::lib::details::llvm_jit_call_raw_from_generated_wasm_abi_bridge, true>(
                state,
                bridge_function_type,
                {module_address,
                 ::llvm::ConstantInt::get(llvm_intptr_type, func_index),
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
        [&](llvm_jit_runtime_raw_call_buffers_t const& raw_call_buffers) constexpr noexcept -> ::llvm::CallBase*
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
            return apply_llvm_jit_raw_entry_calling_conv(emit_runtime_local_func_llvm_jit_may_throw_call(state, raw_entry_function_type,
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
#include "single_func_retained_tiered_local_target_emit.h"

[[nodiscard]] inline constexpr llvm_jit_lazy_typed_target_emit_result_t
    emit_runtime_local_func_llvm_jit_lazy_typed_target_wasm_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                 ::std::size_t local_function_index,
                                                                 ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type,
                                                                 llvm_jit_prepared_wasm_call_operands_t const& prepared_call,
                                                                 ::llvm::StringRef param_buffer_name,
                                                                 ::llvm::StringRef result_buffer_name,
                                                                 bool retain_static_local_identity = false) noexcept
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
        // Keep the known-entry fast path, but resolve this generation's entry
        // by a stable symbol again when another process loads the object.
        auto const entry_symbol{::uwvm2::utils::container::u8concat_uwvm(
            target_table_symbol_name, u8"_known_entry_", local_function_index)};
        auto typed_entry_function_ptr{get_llvm_external_host_object_pointer(ir_builder,
            known_typed_entry_address, ::llvm::Type::getInt8Ty(llvm_context),
            ::uwvm2::utils::container::u8string_view{entry_symbol.data(), entry_symbol.size()})};
        if(typed_entry_function_ptr == nullptr) [[unlikely]] { return {}; }
        auto typed_call{emit_llvm_jit_typed_wasm_call(ir_builder, wasm_function_type,
            callee_function_type, typed_entry_function_ptr, {prepared_call.arguments.data(), prepared_call.arguments.size()},
            [&](::llvm::FunctionType* type, ::llvm::Value* callee, ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept
            { return emit_runtime_local_func_llvm_jit_may_throw_call(state, type, callee, arguments); })};
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
    if(retain_static_local_identity)
    { attach_retained_tiered_local_target_identity(state, *typed_entry_address, local_function_index, wasm_function_type); }

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
    auto fast_call{emit_llvm_jit_typed_wasm_call(ir_builder, wasm_function_type,
        callee_function_type, typed_entry_function_ptr, {prepared_call.arguments.data(), prepared_call.arguments.size()},
            [&](::llvm::FunctionType* type, ::llvm::Value* callee, ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept
            { return emit_runtime_local_func_llvm_jit_may_throw_call(state, type, callee, arguments); })};
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

[[nodiscard]] inline constexpr ::llvm::Align
get_llvm_jit_local_imported_global_byte_buffer_alignment(runtime_operand_stack_value_type value_type) noexcept
{
    if(get_runtime_wasm_value_type_encoding(value_type) == 0x69u)
    { return ::llvm::Align{alignof(runtime_wasm_global_ref)}; }
    switch(value_type)
    {
        case runtime_operand_stack_value_type::v128: return ::llvm::Align{alignof(runtime_wasm_v128)};
        case runtime_operand_stack_value_type::funcref: return ::llvm::Align{alignof(runtime_wasm_funcref)};
        case runtime_operand_stack_value_type::externref: return ::llvm::Align{alignof(runtime_wasm_externref)};
        [[unlikely]] default: return ::llvm::Align{1u};
    }
}

// Allocate the exact byte extent used by a v128/reference local-imported global bridge. The alloca is intentionally an
// i8 array instead of the internal opaque integer: only generated loads/stores interpret its bits as an LLVM value.
[[nodiscard]] inline constexpr ::llvm::AllocaInst*
    create_llvm_jit_local_imported_global_byte_buffer(::llvm::IRBuilder<>& ir_builder,
                                                      runtime_operand_stack_value_type value_type,
                                                      ::llvm::Type* llvm_value_type,
                                                      ::llvm::StringRef name) noexcept
{
    if(get_llvm_jit_local_imported_global_bridge_abi(value_type) != llvm_jit_local_imported_global_bridge_abi::byte_buffer ||
       llvm_value_type == nullptr || llvm_value_type != get_llvm_type_from_wasm_value_type(ir_builder.getContext(), value_type)) [[unlikely]]
    {
        return nullptr;
    }

    auto const byte_size{get_runtime_wasm_value_type_abi_size(value_type)};
    if(byte_size == 0uz) [[unlikely]] { return nullptr; }
    auto byte_buffer_type{::llvm::ArrayType::get(::llvm::Type::getInt8Ty(ir_builder.getContext()), static_cast<::std::uint_least64_t>(byte_size))};
    auto byte_buffer{create_llvm_jit_entry_block_alloca(ir_builder, byte_buffer_type, nullptr, name)};
    if(byte_buffer != nullptr) { byte_buffer->setAlignment(get_llvm_jit_local_imported_global_byte_buffer_alignment(value_type)); }
    return byte_buffer;
}

// Emit the local-imported global.get bridge for the resolved type. Numeric scalars retain the direct typed return ABI;
// v128/references use an out-buffer and are loaded into their internal byte-vector/opaque-integer form after the host returns.
[[nodiscard]] inline constexpr ::llvm::Value*
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

    auto const module_symbol_name{get_llvm_local_imported_global_module_symbol_name(runtime_module, global_index)};
    auto module_pointer{get_llvm_external_host_object_pointer(ir_builder,
                                                              reinterpret_cast<::std::uintptr_t>(global_access_info.local_imported_module_ptr),
                                                              ::llvm::Type::getInt8Ty(llvm_context),
                                                              ::uwvm2::utils::container::u8string_view{module_symbol_name.data(), module_symbol_name.size()})};
    if(module_pointer == nullptr) [[unlikely]] { return nullptr; }

    auto module_address{ir_builder.CreatePtrToInt(module_pointer, llvm_intptr_type, get_llvm_string_ref(u8"global.local_imported.module.addr"))};
    auto global_index_value{::llvm::ConstantInt::get(llvm_intptr_type, global_access_info.local_imported_global_index)};

    switch(get_llvm_jit_local_imported_global_bridge_abi(global_access_info.value_type))
    {
        case llvm_jit_local_imported_global_bridge_abi::scalar_value:
        {
            auto bridge_function_type{::llvm::FunctionType::get(get_llvm_jit_scalar_bits_type(llvm_global_type), {llvm_intptr_type, llvm_intptr_type}, false)};
            ::llvm::Value* bridge_arguments_array[]{module_address, global_index_value};
            auto result{emit_runtime_local_func_llvm_jit_runtime_scalar_bridge_call<llvm_jit_local_imported_global_get_bridge<runtime_wasm_i32>,
                                                                               llvm_jit_local_imported_global_get_bridge<runtime_wasm_i64>,
                                                                               llvm_jit_local_imported_global_get_bridge<runtime_wasm_i32>,
                                                                               llvm_jit_local_imported_global_get_bridge<runtime_wasm_i64>>(
                state,
                global_access_info.value_type,
                bridge_function_type,
                ::llvm::ArrayRef<::llvm::Value*>{bridge_arguments_array})};
            return result == nullptr ? nullptr : ir_builder.CreateBitCast(result, llvm_global_type);
        }
        case llvm_jit_local_imported_global_bridge_abi::byte_buffer:
        {
            auto byte_buffer{create_llvm_jit_local_imported_global_byte_buffer(
                ir_builder, global_access_info.value_type, llvm_global_type, get_llvm_string_ref(u8"global.local_imported.get.bytes"))};
            if(byte_buffer == nullptr) [[unlikely]] { return nullptr; }
            auto byte_buffer_address{
                ir_builder.CreatePtrToInt(byte_buffer, llvm_intptr_type, get_llvm_string_ref(u8"global.local_imported.get.bytes.addr"))};
            auto bridge_function_type{::llvm::FunctionType::get(
                ::llvm::Type::getVoidTy(llvm_context), {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type}, false)};
            ::llvm::Value* bridge_arguments_array[]{module_address, global_index_value, byte_buffer_address};
            if(emit_runtime_local_func_llvm_jit_runtime_bridge_call<llvm_jit_local_imported_global_get_byte_buffer_bridge>(
                   state, bridge_function_type, ::llvm::ArrayRef<::llvm::Value*>{bridge_arguments_array}) == nullptr) [[unlikely]]
            {
                return nullptr;
            }
            auto loaded_value{ir_builder.CreateLoad(llvm_global_type, byte_buffer, get_llvm_string_ref(u8"global.local_imported.get"))};
            loaded_value->setAlignment(get_llvm_jit_local_imported_global_byte_buffer_alignment(global_access_info.value_type));
            return loaded_value;
        }
        [[unlikely]] case llvm_jit_local_imported_global_bridge_abi::unsupported:
            return nullptr;
    }
    return nullptr;
}

// Emit the local-imported global.set bridge for the resolved type. Numeric scalars retain the direct typed argument ABI;
// v128/references are stored into an input byte buffer whose address crosses the host boundary.
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

    auto const module_symbol_name{get_llvm_local_imported_global_module_symbol_name(runtime_module, global_index)};
    auto module_pointer{get_llvm_external_host_object_pointer(ir_builder,
                                                              reinterpret_cast<::std::uintptr_t>(global_access_info.local_imported_module_ptr),
                                                              ::llvm::Type::getInt8Ty(llvm_context),
                                                              ::uwvm2::utils::container::u8string_view{module_symbol_name.data(), module_symbol_name.size()})};
    if(module_pointer == nullptr) [[unlikely]] { return nullptr; }

    auto module_address{ir_builder.CreatePtrToInt(module_pointer, llvm_intptr_type, get_llvm_string_ref(u8"global.local_imported.module.addr"))};
    auto global_index_value{::llvm::ConstantInt::get(llvm_intptr_type, global_access_info.local_imported_global_index)};

    switch(get_llvm_jit_local_imported_global_bridge_abi(global_access_info.value_type))
    {
        case llvm_jit_local_imported_global_bridge_abi::scalar_value:
        {
            auto bridge_function_type{
                ::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context), {llvm_intptr_type, llvm_intptr_type, get_llvm_jit_scalar_bits_type(llvm_value_type)}, false)};
            ::llvm::Value* bridge_arguments_array[]{module_address, global_index_value, ir_builder.CreateBitCast(value, get_llvm_jit_scalar_bits_type(llvm_value_type))};
            return emit_runtime_local_func_llvm_jit_runtime_scalar_bridge_call<llvm_jit_local_imported_global_set_bridge<runtime_wasm_i32>,
                                                                               llvm_jit_local_imported_global_set_bridge<runtime_wasm_i64>,
                                                                               llvm_jit_local_imported_global_set_bridge<runtime_wasm_i32>,
                                                                               llvm_jit_local_imported_global_set_bridge<runtime_wasm_i64>>(
                state,
                global_access_info.value_type,
                bridge_function_type,
                ::llvm::ArrayRef<::llvm::Value*>{bridge_arguments_array});
        }
        case llvm_jit_local_imported_global_bridge_abi::byte_buffer:
        {
            auto byte_buffer{create_llvm_jit_local_imported_global_byte_buffer(
                ir_builder, global_access_info.value_type, llvm_value_type, get_llvm_string_ref(u8"global.local_imported.set.bytes"))};
            if(byte_buffer == nullptr) [[unlikely]] { return nullptr; }
            auto store{ir_builder.CreateStore(value, byte_buffer)};
            store->setAlignment(get_llvm_jit_local_imported_global_byte_buffer_alignment(global_access_info.value_type));
            auto byte_buffer_address{
                ir_builder.CreatePtrToInt(byte_buffer, llvm_intptr_type, get_llvm_string_ref(u8"global.local_imported.set.bytes.addr"))};
            auto bridge_function_type{::llvm::FunctionType::get(
                ::llvm::Type::getVoidTy(llvm_context), {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type}, false)};
            ::llvm::Value* bridge_arguments_array[]{module_address, global_index_value, byte_buffer_address};
            return emit_runtime_local_func_llvm_jit_runtime_bridge_call<llvm_jit_local_imported_global_set_byte_buffer_bridge>(
                state, bridge_function_type, ::llvm::ArrayRef<::llvm::Value*>{bridge_arguments_array});
        }
        [[unlikely]] case llvm_jit_local_imported_global_bridge_abi::unsupported:
            return nullptr;
    }
    return nullptr;
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


// Typed select already had inline physical lowering. Consume the first-typed
// DATA through that SAME physical LLVM select body, with no extra raw decoding.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_typed_select(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_typed_select_event const& event) noexcept
{
    namespace v = ::uwvm2::validation::standard::wasm3;
    if(!state.valid || event.opcode != 0x1cu || !v::typed_select_carrier_consistent(event.result_type, event.result_carrier) ||
       event.source_bytes < 3uz || event.source_bytes > 12uz || state.local_func_storage_ptr == nullptr ||
       state.control_stack.empty() || event.source_offset == SIZE_MAX || state.current_wasm_op_offset != event.source_offset ||
       state.unreachable_control_depth > SIZE_MAX - state.control_stack.size() ||
       event.control_depth != state.control_stack.size() + state.unreachable_control_depth ||
       (state.control_stack.back().is_reachable && event.stack_polymorphic)) { return false; }
    auto const& function{*state.local_func_storage_ptr};
    // [actual retained body begin ... complete expression] | end
    // [safe single owner] one-past: source subtraction only; no bytes read or pointer advance.
    if(function.code_begin == nullptr || function.code_end == nullptr || function.code_end < function.code_begin)
    { return false; }
    auto const bytes{static_cast<::std::size_t>(function.code_end - function.code_begin)};
    if(event.source_offset >= bytes || event.source_bytes > bytes - event.source_offset) { return false; }
    if(state.control_stack.back().is_reachable)
    {
        if(state.operand_stack.size() < 3uz) { return false; }
        // Physical SSA owner/ABI consistency, not another abstract Wasm matcher.
        // Full arity precedes the two fixed from-top indexed metadata reads.
        auto const size{state.operand_stack.size()};
        if(static_cast<unsigned>(state.operand_stack[size - 2uz].type) != event.result_carrier ||
           static_cast<unsigned>(state.operand_stack[size - 3uz].type) != event.result_carrier) { return false; }
    }
    return try_emit_runtime_local_func_llvm_jit_select(state);
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
    if(!emit_runtime_local_func_llvm_jit_checkpoint_assignment_flag(
        state, state.checkpoint_executed_local_flags, local_index_uz)) { return false; }
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
    if(!emit_runtime_local_func_llvm_jit_checkpoint_assignment_flag(
        state, state.checkpoint_executed_local_flags, local_index_uz)) { return false; }
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
    if(runtime_module_ptr == nullptr ||
       (state.pending_numeric_plan != nullptr && state.pending_numeric_plan->module != runtime_module_ptr))
    [[unlikely]] { return false; }

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
                                                            global_access_info.value_type,
                                                            state.pending_numeric_plan != nullptr)};
        if(global_pointer == nullptr) [[unlikely]] { return false; }

        auto loaded_value{ir_builder.CreateLoad(llvm_global_type, global_pointer, get_llvm_string_ref(u8"global.get"))};
        if(loaded_value == nullptr) [[unlikely]] { return false; }
        loaded_value->setAlignment(get_llvm_global_storage_alignment());

        state.operand_stack.push_back({.type = global_access_info.value_type, .value = loaded_value});
        return true;
    }

    auto bridge_call{
        emit_runtime_local_func_llvm_jit_local_imported_global_get_bridge_call(state, *runtime_module_ptr, global_index, global_access_info, llvm_global_type)};
    if(bridge_call == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = global_access_info.value_type, .value = bridge_call});
    return true;
}

[[nodiscard]] inline ::std::uint32_t llvm_jit_global_retain_reference_bridge(
    ::std::uintptr_t storage_address, ::std::uintptr_t value_address) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    using global_storage = ::uwvm2::object::global::wasm_global_storage_t;
    static_assert(::std::is_standard_layout_v<global_storage> && offsetof(global_storage, storage) == 0uz);
    if(storage_address == 0u || value_address == 0u)
    { return static_cast<::std::uint32_t>(storage::gc_object_status::invalid_store); }
    // [VM-owned global storage][generated native-width reference slot]
    // [safe                  ][safe                                  ] the JIT emitter
    // passes only VM-generated addresses; the reference payload remains untrusted.
    auto const* target{reinterpret_cast<global_storage const*>(storage_address)};
    storage::gc_reference reference{};
    ::std::memcpy(::std::addressof(reference), reinterpret_cast<void const*>(value_address), sizeof(reference));
    auto const* store{static_cast<storage::gc_object_store const*>(target->ref_lease_store)};
    return static_cast<::std::uint32_t>(storage::uwvm2_gc_retain_reference(store, ::std::addressof(reference)));
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
    if(runtime_module_ptr == nullptr ||
       (state.pending_numeric_plan != nullptr && state.pending_numeric_plan->module != runtime_module_ptr))
    [[unlikely]] { return false; }

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
                                                            global_access_info.value_type,
                                                            state.pending_numeric_plan != nullptr)};
        if(global_pointer == nullptr) [[unlikely]] { return false; }

        if(global_access_info.value_type == runtime_operand_stack_value_type::funcref ||
           global_access_info.value_type == runtime_operand_stack_value_type::externref ||
           get_runtime_wasm_value_type_encoding(global_access_info.value_type) == 0x69u)
        {
            namespace storage = ::uwvm2::uwvm::runtime::storage;
            auto& builder{*state.ir_builder};
            auto* intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
            auto* ref_slot{create_llvm_jit_entry_block_alloca(builder, builder.getInt8Ty(),
                ::llvm::ConstantInt::get(intptr, sizeof(storage::gc_reference)),
                get_llvm_string_ref(u8"global.set.reference"))};
            if(ref_slot == nullptr) { return false; }
            ref_slot->setAlignment(::llvm::Align{alignof(storage::gc_reference)});
            // [generated reference slot] exact carrier width is validated by the
            // LLVM Wasm type lowerer; publish all bits before the retaining call.
            builder.CreateStore(value.value, ref_slot)->setAlignment(::llvm::Align{1u});
            auto* bridge_type{::llvm::FunctionType::get(builder.getInt32Ty(), {intptr, intptr}, false)};
            auto* status{emit_runtime_local_func_llvm_jit_runtime_bridge_call<llvm_jit_global_retain_reference_bridge>(
                state, bridge_type,
                {builder.CreatePtrToInt(global_pointer, intptr), builder.CreatePtrToInt(ref_slot, intptr)})};
            if(status == nullptr) { return false; }
            emit_llvm_conditional_trap(*state.llvm_module, builder,
                builder.CreateICmpEQ(status, builder.getInt32(static_cast<unsigned>(storage::gc_object_status::out_of_memory))),
                ::uwvm2::runtime::lib::llvm_jit_trap_kind::gc_allocation_failure);
            emit_llvm_conditional_trap(*state.llvm_module, builder,
                builder.CreateICmpNE(status, builder.getInt32(0u)),
                ::uwvm2::runtime::lib::llvm_jit_trap_kind::runtime_invariant_failure);
        }

        state.ir_builder->CreateStore(value.value, global_pointer)->setAlignment(get_llvm_global_storage_alignment());
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

// A direct self tail call is a backedge, independent of the host ABI and of
// full/lazy/tiered entry wrappers. Existing logical/native frames are reused.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_return_call_self(
    runtime_local_func_llvm_jit_emit_state_t& state, validation_module_traits_t::wasm_u32 func_index) noexcept
{
    if(!state.valid || state.control_stack.empty() || state.local_func_storage_ptr == nullptr) { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.local_func_storage_ptr->function_index != static_cast<::std::size_t>(func_index) ||
       state.self_tail_entry_block == nullptr) { return false; }
    auto const type{state.local_func_storage_ptr->function_type_ptr};
    if(type == nullptr) { return false; }
    auto const prepared{prepare_runtime_local_func_llvm_jit_wasm_call_operands(state, *type)};
    if(!prepared.valid || prepared.arguments.size() > state.local_pointers.size()) { return false; }
    // Capture all SSA arguments before changing any local. Argument permutation
    // and aliases such as return_call f(local.get 1, local.get 0) stay correct.
    for(::std::size_t i{}; i != state.local_pointers.size(); ++i)
    {
        auto const value{i < prepared.arguments.size() ? prepared.arguments[i] :
            get_llvm_zero_constant_from_wasm_value_type(*state.llvm_context_holder, state.local_types[i])};
        if(value == nullptr) { return false; }
        state.ir_builder->CreateStore(value, state.local_pointers[i]);
    }
    if(!emit_runtime_local_func_llvm_jit_checkpoint_reset_initialization_flags(
        state, state.checkpoint_executed_local_flags)) { return false; }
    state.ir_builder->CreateBr(state.self_tail_entry_block);
    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

// A host-only raw target needs a typed adapter so its packed parameter/result
// buffers outlive the retiring Wasm frame. Defined Wasm targets MUST be resolved
// to typed entries first; otherwise adapters would accumulate along a tail chain.
template <bool DirectImport = false>
[[nodiscard]] inline constexpr ::llvm::Function* create_llvm_jit_host_tail_adapter(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_type) noexcept
{
    auto& context{*state.llvm_context_holder};
    auto const type{get_llvm_function_type_from_wasm_function_type(context, wasm_type)};
    auto const layout{get_runtime_wasm_call_abi_layout(wasm_type)};
    if(type == nullptr || !layout.valid || type->getReturnType()->isAggregateType()) { return nullptr; }
    auto const intptr{::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    ::uwvm2::utils::container::vector<::llvm::Type*> parameters{intptr, intptr};
    for(auto parameter: type->params()) { parameters.push_back(parameter); }
    auto const adapter_type{::llvm::FunctionType::get(type->getReturnType(), {parameters.data(), parameters.size()}, false)};
    // [LLVM module ownership] retains function/blocks through object emission.
    // [safe                 ]; no host/guest address is derived from this pointer.
    auto const adapter{::llvm::Function::Create(adapter_type, ::llvm::Function::InternalLinkage,
                                                get_llvm_string_ref(u8"tail.host.adapter"), state.llvm_module)};
    apply_llvm_jit_wasm_calling_conv(*adapter);
    adapter->addFnAttr(::llvm::Attribute::NoInline);
    if(state.debug_activation_enabled) { adapter->addFnAttr("uwvm.debug.activation"); }
    if(state.emit_unwind_call_stack_frames) { apply_llvm_jit_unwind_call_stack_function_attrs(*adapter); }
    else if(state.native_guest_exceptions) { adapter->setUWTableKind(::llvm::UWTableKind::Sync); }
    auto const block{::llvm::BasicBlock::Create(context, get_llvm_string_ref(u8"entry"), adapter)};
    ::llvm::IRBuilder<> builder(block);
    llvm_jit_gc_root_frame_emit_state_t adapter_roots{};
    if(!prepare_runtime_llvm_jit_gc_root_frame(builder, *adapter, adapter_roots, state.emit_precise_gc_root_frames))
    { return nullptr; }
    ::uwvm2::utils::container::vector<::llvm::Value*> arguments{};
    for(::std::size_t i{}; i != layout.parameter_count; ++i) { arguments.push_back(adapter->getArg(i + 2uz)); }
    if(state.emit_precise_gc_root_frames)
    {
        ::std::size_t count{};
        for(::std::size_t index{}; index != layout.parameter_count; ++index)
        {
            auto const parameter{static_cast<runtime_operand_stack_value_type>(wasm_type.parameter.begin[index])};
            if(llvm_jit_gc_root_carrier_type(parameter) &&
               !emit_runtime_llvm_jit_gc_root_slot(builder, adapter_roots, arguments[index], count++)) { return nullptr; }
        }
        if(!publish_runtime_llvm_jit_gc_root_count(builder, adapter_roots, count) ||
           !finalize_runtime_llvm_jit_gc_root_frame(adapter_roots, true)) { return nullptr; }
    }
    auto const buffers{emit_runtime_raw_call_buffers(builder, wasm_type, {arguments.data(), arguments.size()},
        get_llvm_string_ref(u8"tail.host.params"), get_llvm_string_ref(u8"tail.host.results"),
        layout.result_count > 1uz ? adapter->getArg(adapter->arg_size() - 1uz) : nullptr)};
    if(!buffers.valid) { return nullptr; }
    if constexpr(DirectImport)
    {
        auto const bridge_type{get_llvm_runtime_raw_call_bridge_function_type(context)};
        auto const bridge{runtime_local_func_llvm_jit_uses_checkpoint_effects(state) ?
            get_llvm_runtime_bridge_function_symbol_value<
                ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_call_raw_from_generated_wasm_abi_bridge>(builder, bridge_type) :
            get_llvm_runtime_bridge_function_symbol_value<
                ::uwvm2::runtime::lib::details::llvm_jit_call_raw_from_generated_wasm_abi_bridge>(builder, bridge_type)};
        if(bridge == nullptr) { return nullptr; }
        auto const call{emit_runtime_llvm_jit_rooted_host_adapter_call(state, builder, bridge_type, bridge,
            {adapter->getArg(0u), adapter->getArg(1u), buffers.result_buffer_address,
             ::llvm::ConstantInt::get(intptr, layout.result_bytes), buffers.param_buffer_address,
             ::llvm::ConstantInt::get(intptr, layout.parameter_bytes)}, adapter_roots)};
        if(call == nullptr) { return nullptr; }
        apply_llvm_jit_host_calling_conv(call);
    }
    else
    {
        auto const raw_type{get_llvm_runtime_raw_call_target_entry_function_type(context)};
        // [engine-provided executable raw entry] paired with the checked host context.
        // [safe                                ]; guest selector never supplies an address.
        auto const raw_entry{builder.CreateIntToPtr(adapter->getArg(0u), get_llvm_pointer_type(raw_type))};
        ::llvm::CallBase* call{};
        if(state.debug_activation_enabled)
        {
            auto const debug_type{get_llvm_runtime_raw_call_bridge_function_type(context)};
            auto const debug_bridge{runtime_local_func_llvm_jit_uses_checkpoint_effects(state) ?
                get_llvm_runtime_bridge_function_symbol_value<
                    ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_raw_target_abi_bridge>(builder, debug_type) :
                get_llvm_runtime_bridge_function_symbol_value<
                    ::uwvm2::runtime::lib::details::llvm_jit_debug_raw_target_abi_bridge>(builder, debug_type)};
            if(debug_bridge == nullptr) { return nullptr; }
            call = emit_runtime_llvm_jit_rooted_host_adapter_call(state, builder, debug_type, debug_bridge,
                {adapter->getArg(0u), adapter->getArg(1u), buffers.result_buffer_address,
                 ::llvm::ConstantInt::get(intptr, layout.result_bytes), buffers.param_buffer_address,
                 ::llvm::ConstantInt::get(intptr, layout.parameter_bytes)}, adapter_roots);
            if(call != nullptr) { apply_llvm_jit_host_calling_conv(call); }
        }
        else
        {
            call = emit_runtime_llvm_jit_rooted_host_adapter_call(state, builder, raw_type, raw_entry,
                {adapter->getArg(1u), buffers.result_buffer_address, ::llvm::ConstantInt::get(intptr, layout.result_bytes),
                 buffers.param_buffer_address, ::llvm::ConstantInt::get(intptr, layout.parameter_bytes)}, adapter_roots);
            if(call != nullptr) { apply_llvm_jit_raw_entry_calling_conv(call); }
        }
        if(call == nullptr) { return nullptr; }
    }
    if(type->getReturnType()->isVoidTy())
    {
        if(!leave_runtime_llvm_jit_gc_root_frame(builder, adapter_roots)) { return nullptr; }
        builder.CreateRetVoid();
    }
    else
    {
        auto const result{emit_runtime_raw_call_result_value(builder, wasm_type, buffers, get_llvm_string_ref(u8"tail.host.result"))};
        if(result == nullptr) { return nullptr; }
        if(!leave_runtime_llvm_jit_gc_root_frame(builder, adapter_roots)) { return nullptr; }
        builder.CreateRet(result);
    }
    if(!verify_llvm_jit_function(*adapter, state.verify_llvm_jit_ir)) { return nullptr; }
    return adapter;
}

// Import forwarding may end in another module or in a host leaf. Resolve a
// typed Wasm target before retiring the caller; host buffers belong to the
// tail-entered adapter, never to the outgoing Wasm frame.
[[nodiscard]] inline constexpr bool try_emit_llvm_jit_import_return_call(
    runtime_local_func_llvm_jit_emit_state_t& state, validation_module_traits_t::wasm_u32 func_index) noexcept
{
    if(state.llvm_function->getCallingConv() != ::llvm::CallingConv::Tail) { return false; }
    auto const module{state.local_func_storage_ptr->runtime_module_ptr};
    auto const wasm_type{resolve_runtime_callee_function_type(*module, func_index)};
    if(wasm_type == nullptr) { return false; }
    auto& context{*state.llvm_context_holder}; auto& builder{*state.ir_builder};
    auto const type{get_llvm_function_type_from_wasm_function_type(context, *wasm_type)};
    if(type == nullptr || get_llvm_jit_typed_calling_conv(*type) != ::llvm::CallingConv::Tail ||
       type->getReturnType() != state.llvm_function->getReturnType()) { return false; }
    auto prepared{prepare_runtime_local_func_llvm_jit_wasm_call_operands(state, *wasm_type)};
    if(!prepared.valid) { return false; }
    if(prepared.abi_layout.result_count > 1uz)
    {
        // [surviving non-tail caller's result_bytes ...] the checked Wasm result
        // [safe                                      ] tuple matches; no retired local escapes.
        prepared.arguments.push_back(state.llvm_function->getArg(state.llvm_function->arg_size() - 1uz));
    }
    auto const intptr{::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto const module_name{get_llvm_runtime_module_object_symbol_name(*module)};
    auto const module_address{get_llvm_external_host_object_address(builder, reinterpret_cast<::std::uintptr_t>(module),
        ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()})};
    if(module_address == nullptr) { return false; }
    auto const index{::llvm::ConstantInt::get(intptr, func_index)};
    auto const resolver_type{::llvm::FunctionType::get(intptr, {intptr, intptr}, false)};
    auto const address{emit_runtime_local_func_llvm_jit_runtime_bridge_call<
        ::uwvm2::runtime::lib::details::llvm_jit_resolve_tail_target_abi_bridge>(state, resolver_type, {module_address, index})};
    if(address == nullptr) { return false; }
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
    if(!sealed_compact_codegen::retire(state)) { return false; }
#endif
    if(!leave_runtime_llvm_jit_gc_root_frame(builder, state.gc_root_frame)) { return false; }
    if(state.emit_call_stack_frames && !emit_runtime_local_func_llvm_jit_call_stack_pop(builder)) { return false; }
    auto const typed_block{::llvm::BasicBlock::Create(context, get_llvm_string_ref(u8"tail.import.typed"), state.llvm_function)};
    auto const host_block{::llvm::BasicBlock::Create(context, get_llvm_string_ref(u8"tail.import.host"), state.llvm_function)};
    builder.CreateCondBr(builder.CreateICmpEQ(address, ::llvm::ConstantInt::get(intptr, 0u)), host_block, typed_block);
    builder.SetInsertPoint(typed_block);
    if(!emit_runtime_local_func_llvm_jit_debug_activation_leave(builder, state, 1u)) { return false; }
    // [published typed entry] owned by the active target module; Wasm import
    // [safe                ] signature and tail-compatible ABI checked above.
    auto const entry{builder.CreateIntToPtr(address, get_llvm_pointer_type(type))};
    auto const transfer{apply_llvm_jit_wasm_calling_conv(builder.CreateCall(type, entry, {prepared.arguments.data(), prepared.arguments.size()}))};
    transfer->setTailCallKind(::llvm::CallInst::TCK_MustTail);
    if(type->getReturnType()->isVoidTy()) { builder.CreateRetVoid(); }
    else { builder.CreateRet(transfer); }
    builder.SetInsertPoint(host_block);
    if(!emit_runtime_local_func_llvm_jit_debug_activation_leave(builder, state, 2u)) { return false; }
    auto const adapter{create_llvm_jit_host_tail_adapter<true>(state, *wasm_type)};
    if(adapter == nullptr) { return false; }
    ::uwvm2::utils::container::vector<::llvm::Value*> arguments{module_address, index};
    for(auto argument: prepared.arguments) { arguments.push_back(argument); }
    auto const host_transfer{apply_llvm_jit_wasm_calling_conv(builder.CreateCall(adapter, {arguments.data(), arguments.size()}))};
    host_transfer->setTailCallKind(::llvm::CallInst::TCK_MustTail);
    if(type->getReturnType()->isVoidTy()) { builder.CreateRetVoid(); }
    else { builder.CreateRet(host_transfer); }
    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

// Emit a native tail transfer. tailcc allows different scalar/vector parameter
// lists; other conventions require identical LLVM prototypes. Multi-value results
// forward the surviving caller's explicit output address. Never degrade to call plus return.
#include "single_func_pending_numeric_tail_emit.h"

[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_return_call(
    runtime_local_func_llvm_jit_emit_state_t& state, validation_module_traits_t::wasm_u32 func_index) noexcept
{
    if(!state.valid || state.control_stack.empty() || state.local_func_storage_ptr == nullptr) { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.pending_numeric_plan != nullptr) { return try_emit_pending_numeric_return_call(state, func_index); }
    if(!state.emit_debug_safe_points && state.debug_full_patchable_typed_target_base_address == 0u &&
       state.local_func_storage_ptr->function_index == static_cast<::std::size_t>(func_index))
    { return try_emit_runtime_local_func_llvm_jit_return_call_self(state, func_index); }
    if(state.llvm_function == nullptr ||
       (state.llvm_function != state.llvm_public_entry_function && state.llvm_function->getCallingConv() != ::llvm::CallingConv::Tail) ||
       (state.route_wasm_calls_through_runtime_bridge && state.lazy_defined_typed_entry_target_base_address == 0u)) { return false; }
    auto const module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr) { return false; }
    auto const target{resolve_runtime_direct_callee(*module, func_index, state.local_func_storage_ptr->compiler_registry)};
    if(!target.state_valid) { return false; }
    if(!target.direct_callable) { return try_emit_llvm_jit_import_return_call(state, func_index); }
    if(target.function_type_ptr == nullptr) { return false; }
    auto const callee_type{get_llvm_function_type_from_wasm_function_type(*state.llvm_context_holder, *target.function_type_ptr)};
    if(callee_type == nullptr || callee_type->getReturnType()->isAggregateType() ||
       callee_type->getReturnType() != state.llvm_function->getReturnType() ||
       get_llvm_jit_typed_calling_conv(*callee_type) != state.llvm_function->getCallingConv() ||
       (state.llvm_function->getCallingConv() != ::llvm::CallingConv::Tail &&
        callee_type != state.llvm_function->getFunctionType())) { return false; }
    // An aggregate parameter may be lowered through caller-owned temporary
    // storage. Accept only SSA scalar/vector arguments with no stack addresses.
    for(auto const type: callee_type->params()) { if(type->isAggregateType()) { return false; } }
    auto prepared{prepare_runtime_local_func_llvm_jit_wasm_call_operands(state, *target.function_type_ptr)};
    if(!prepared.valid) { return false; }
    if(prepared.abi_layout.result_count > 1uz)
    {
        // [surviving non-tail caller's result_bytes ...] the checked Wasm result
        // [safe                                      ] tuple matches; no retired local escapes.
        prepared.arguments.push_back(state.llvm_function->getArg(state.llvm_function->arg_size() - 1uz));
    }
    auto& ir_builder{*state.ir_builder};
    auto& context{*state.llvm_context_holder};
    ::llvm::Value* callee{};
    if(state.debug_full_patchable_typed_target_base_address != 0u)
    {
        // [validated same-module target] slot bounds/type checked by the helper.
        // [safe                        ] self-tail also reloads instead of retaining an old body backedge.
        callee = emit_runtime_local_func_llvm_jit_debug_full_typed_target(state, *module, target.func_index, callee_type);
    }
    else if(state.lazy_defined_typed_entry_target_base_address != 0u)
    {
        auto const imports{module->imported_function_vec_storage.size()};
        if(target.func_index < imports) { return false; }
        auto const local_index{static_cast<::std::size_t>(target.func_index) - imports};
        if(local_index >= state.lazy_defined_typed_entry_target_count) { return false; }
        auto const intptr{::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t)*8u))};
        auto const table_name{get_llvm_lazy_typed_entry_target_table_symbol_name(*module)};
        auto const base{get_llvm_external_host_object_pointer(ir_builder,
            state.lazy_defined_typed_entry_target_base_address, intptr, ::uwvm2::utils::container::u8string_view{table_name.data(), table_name.size()})};
        if(base == nullptr) { return false; }
        // [published target slots ...] count; local_index was bounded above.
        auto const slot{ir_builder.CreateInBoundsGEP(intptr, base, ::llvm::ConstantInt::get(intptr, local_index))};
        auto const loaded{ir_builder.CreateLoad(intptr, slot, get_llvm_string_ref(u8"tail.target"))};
        loaded->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
        loaded->setVolatile(true);
        if(state.lazy_defined_targets_are_atomic) { loaded->setAtomic(::llvm::AtomicOrdering::Acquire); }
        auto const fast{ir_builder.GetInsertBlock()};
        auto const slow{::llvm::BasicBlock::Create(context, get_llvm_string_ref(u8"tail.materialize"), state.llvm_function)};
        auto const ready{::llvm::BasicBlock::Create(context, get_llvm_string_ref(u8"tail.ready"), state.llvm_function)};
        ir_builder.CreateCondBr(ir_builder.CreateICmpEQ(loaded, ::llvm::ConstantInt::get(intptr, 0u)), slow, ready);
        ir_builder.SetInsertPoint(slow);
        auto const module_name{get_llvm_runtime_module_object_symbol_name(*module)};
        auto const module_address{get_llvm_external_host_object_address(ir_builder,
            reinterpret_cast<::std::uintptr_t>(module), ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()})};
        if(module_address == nullptr) { return false; }
        auto const resolve_type{::llvm::FunctionType::get(intptr, {intptr, intptr}, false)};
        auto const resolved{emit_runtime_local_func_llvm_jit_runtime_bridge_call<
            ::uwvm2::runtime::lib::details::llvm_jit_resolve_tail_target_abi_bridge>(state, resolve_type,
                {module_address, ::llvm::ConstantInt::get(intptr, target.func_index)})};
        if(resolved == nullptr) { return false; }
        auto const slow_end{ir_builder.GetInsertBlock()};
        ir_builder.CreateBr(ready);
        ir_builder.SetInsertPoint(ready);
        auto const address{ir_builder.CreatePHI(intptr, 2u, get_llvm_string_ref(u8"tail.address"))};
        address->addIncoming(loaded, fast); address->addIncoming(resolved, slow_end);
        callee = ir_builder.CreateIntToPtr(address, get_llvm_pointer_type(callee_type));
        // ^ callee: published executable typed entry, with a checked tail-compatible ABI.
    }
    else
    {
        callee = get_or_create_llvm_wasm_function_declaration(*state.llvm_module, context, *module,
                                                             target.func_index, *target.function_type_ptr);
        // ^ callee: module-owned declaration for the validated local target.
    }
    if(callee == nullptr) { return false; }
    // Instruction mode retires this logical frame before the callee pushes its
    // own. Native-unwind mode emits neither maintenance operation at this edge.
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
    if(!sealed_compact_codegen::retire(state)) { return false; }
#endif
    if(!leave_runtime_llvm_jit_gc_root_frame(*state.ir_builder, state.gc_root_frame)) { return false; }
    if(state.emit_call_stack_frames && !emit_runtime_local_func_llvm_jit_call_stack_pop(*state.ir_builder)) { return false; }
    if(!emit_runtime_local_func_llvm_jit_debug_activation_leave(*state.ir_builder, state, 1u)) { return false; }
    auto const transfer{apply_llvm_jit_wasm_calling_conv(
        state.ir_builder->CreateCall(callee_type, callee, {prepared.arguments.data(), prepared.arguments.size()}))};
    if(transfer == nullptr) { return false; }
    transfer->setTailCallKind(::llvm::CallInst::TCK_MustTail);
    // LLVM requires ret immediately after musttail, returning its exact SSA
    // result. No local address or caller-owned argument buffer escapes this edge.
    if(callee_type->getReturnType()->isVoidTy()) { state.ir_builder->CreateRetVoid(); }
    else { state.ir_builder->CreateRet(transfer); }
    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

#include "single_func_retained_unwind_import_emit.h"

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
    if(!emit_runtime_local_func_llvm_jit_checkpoint_awaiting_direct_call(state, *callee_type_ptr)) { return false; }

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
                                                                                                                  result_buffer_name,
                                                                                                                  static_cast<::std::size_t>(func_index) >=
                                                                                                                      runtime_module_ptr->imported_function_vec_storage.size())};
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
        auto const retained_import{stage_retained_unwind_import_call(state, func_index, *callee_type_ptr,
            prepared_call, emit_lazy_defined_target_call)};
        if(retained_import.handled)
        {
            if(!retained_import.valid) { return false; }
            return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared_call, retained_import.result);
        }
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
        auto const callee_resolution{resolve_runtime_direct_callee(*runtime_module_ptr, func_index, state.local_func_storage_ptr->compiler_registry)};
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
template <bool Tail = false>
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
    auto const address64{::uwvm2::uwvm::runtime::storage::runtime_table_is_address64(*runtime_module_ptr, table_index)};
    if(selector.type != (address64 ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32) || selector.value == nullptr) [[unlikely]] { return false; }

    auto prepared_call{prepare_runtime_local_func_llvm_jit_wasm_call_operands(state, *callee_type_ptr)};
    if(!prepared_call.valid) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    auto llvm_module{state.llvm_module};
    auto& ir_builder{*state.ir_builder};
    auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};

    auto const expected_type_id{resolve_runtime_canonical_type_id(*runtime_module_ptr, type_index)};
    if(expected_type_id == invalid_runtime_canonical_type_id()) [[unlikely]] { return false; }
    auto const expected_type_width{resolve_runtime_call_type_match_width(*runtime_module_ptr, type_index)};
    if(expected_type_width == 0u) [[unlikely]] { return false; }

    auto raw_entry_function_type{get_llvm_runtime_raw_call_target_entry_function_type(llvm_context)};
    auto raw_target_struct_type{get_llvm_runtime_raw_call_target_struct_type(llvm_context)};
    auto table_view_struct_type{get_llvm_runtime_call_indirect_table_view_struct_type(llvm_context)};
    auto typed_entry_function_type{get_llvm_function_type_from_wasm_function_type(llvm_context, *callee_type_ptr)};
    if(raw_entry_function_type == nullptr || raw_target_struct_type == nullptr || table_view_struct_type == nullptr || typed_entry_function_type == nullptr)
        [[unlikely]]
    {
        return false;
    }

    ::llvm::Value* selector_index{};
    ::llvm::LoadInst* entry_address{};
    ::llvm::LoadInst* context_address{};
    ::llvm::LoadInst* encoded_type_id{};
    ::llvm::LoadInst* typed_entry_address{};
    if(state.emit_debug_safe_points)
    {
        // Debug-full compiles before ordered active segments run. A compact
        // snapshot made now can contain null entries that become live later;
        // a prior start may also mutate an imported table on another thread.
        // Resolve a copied target under the same table lock as every debug
        // table operation, then invoke it after the helper releases the lock.
        auto const module_name{get_llvm_runtime_module_object_symbol_name(*runtime_module_ptr)};
        auto const module_address{get_llvm_external_host_object_address(ir_builder,
            reinterpret_cast<::std::uintptr_t>(runtime_module_ptr),
            ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()})};
        auto const target_slot{create_llvm_jit_entry_block_alloca(ir_builder, raw_target_struct_type, nullptr,
            get_llvm_string_ref(u8"debug.call_indirect.target.slot"))};
        if(module_address == nullptr || target_slot == nullptr) [[unlikely]] { return false; }
        target_slot->setAlignment(::llvm::Align{alignof(::uwvm2::uwvm::runtime::storage::llvm_jit_raw_call_target_t)});
        auto const selector_i64{ir_builder.CreateZExtOrTrunc(selector.value, ::llvm::Type::getInt64Ty(llvm_context),
            get_llvm_string_ref(u8"debug.call_indirect.selector.i64"))};
        auto const target_output_address{ir_builder.CreatePtrToInt(target_slot, llvm_intptr_type,
            get_llvm_string_ref(u8"debug.call_indirect.target.address"))};
        auto const resolver_type{::llvm::FunctionType::get(llvm_intptr_type,
            {llvm_intptr_type, llvm_intptr_type, ::llvm::Type::getInt64Ty(llvm_context), llvm_intptr_type}, false)};
        auto const status{emit_runtime_local_func_llvm_jit_runtime_bridge_call<
            ::uwvm2::runtime::lib::details::llvm_jit_debug_call_indirect_target_abi_bridge>(state, resolver_type,
            {module_address, ::llvm::ConstantInt::get(llvm_intptr_type, table_index), selector_i64, target_output_address})};
        if(status == nullptr) [[unlikely]] { return false; }
        emit_llvm_conditional_trap(*llvm_module, ir_builder,
            ir_builder.CreateICmpNE(status, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)),
            ::uwvm2::runtime::lib::llvm_jit_trap_kind::call_indirect_table_out_of_bounds);
        // [0 <= selector < table.size <= SIZE_MAX] was checked by the helper.
        // [safe                                  ] narrowing is now lossless.
        selector_index = ir_builder.CreateZExtOrTrunc(selector.value, llvm_intptr_type,
            get_llvm_string_ref(u8"debug.call_indirect.selector.index"));
        auto const entry_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_slot, 0u)};
        auto const context_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_slot, 1u)};
        auto const type_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_slot, 2u)};
        auto const typed_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_slot, 3u)};
        entry_address = ir_builder.CreateLoad(llvm_intptr_type, entry_ptr);
        context_address = ir_builder.CreateLoad(llvm_intptr_type, context_ptr);
        encoded_type_id = ir_builder.CreateLoad(llvm_i32_type, type_ptr);
        typed_entry_address = ir_builder.CreateLoad(llvm_intptr_type, typed_ptr);
    }
    else
    {
        auto table_view_begin{runtime_module_ptr->llvm_jit_call_indirect_table_views.data()};
        if(table_view_begin == nullptr) [[unlikely]] { return false; }

        // The table view is a compact runtime-maintained snapshot: one entry per table with a target-record base address and
        // current size.  The selected target record then carries raw and optional typed entry addresses.
        auto const table_view_symbol_name{get_llvm_call_indirect_table_view_symbol_name(*runtime_module_ptr)};
        auto table_view_base_ptr{
            get_llvm_external_host_object_pointer(ir_builder,
                                                  reinterpret_cast<::std::uintptr_t>(table_view_begin),
                                                  table_view_struct_type,
                                                  ::uwvm2::utils::container::u8string_view{table_view_symbol_name.data(), table_view_symbol_name.size()})};
        if(table_view_base_ptr == nullptr) [[unlikely]] { return false; }

        // [declared table view] the module/table index was checked above; no guest address participates.
        // Compare at max(Wasm address width, host pointer width), before any truncation for GEP.
        auto llvm_compare_type{::llvm::Type::getIntNTy(llvm_context, address64 && sizeof(::std::uintptr_t) < 8u ? 64u : static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
        auto selector_full{ir_builder.CreateZExtOrTrunc(selector.value, llvm_compare_type, get_llvm_string_ref(u8"call_indirect.selector.full"))};
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
                                   ir_builder.CreateICmpUGE(selector_full, ir_builder.CreateZExtOrTrunc(table_size, llvm_compare_type)),
                                   ::uwvm2::runtime::lib::llvm_jit_trap_kind::call_indirect_table_out_of_bounds);
        emit_llvm_conditional_trap(*llvm_module,
                                   ir_builder,
                                   ir_builder.CreateICmpEQ(table_data_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)),
                                   ::uwvm2::runtime::lib::llvm_jit_trap_kind::call_indirect_null_element);

        // [0 ... selector_full ... table_size) the full-width unsigned bounds test dominates this block.
        // [safe                             ] narrowing cannot discard any nonzero high bits; GEP stays in the live target array.
        selector_index = ir_builder.CreateZExtOrTrunc(selector_full, llvm_intptr_type, get_llvm_string_ref(u8"call_indirect.selector.index"));
        auto target_base_ptr{
            ir_builder.CreateIntToPtr(table_data_address, get_llvm_pointer_type(raw_target_struct_type), get_llvm_string_ref(u8"call_indirect.target.base.ptr"))};
        auto target_ptr{ir_builder.CreateInBoundsGEP(raw_target_struct_type, target_base_ptr, selector_index, get_llvm_string_ref(u8"call_indirect.target.ptr"))};
        auto entry_address_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_ptr, 0u, get_llvm_string_ref(u8"call_indirect.entry.addr.ptr"))};
        auto context_address_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_ptr, 1u, get_llvm_string_ref(u8"call_indirect.context.addr.ptr"))};
        auto encoded_type_id_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_ptr, 2u, get_llvm_string_ref(u8"call_indirect.type.id.ptr"))};
        auto typed_entry_address_ptr{
            ir_builder.CreateStructGEP(raw_target_struct_type, target_ptr, 3u, get_llvm_string_ref(u8"call_indirect.typed.entry.addr.ptr"))};
        entry_address = ir_builder.CreateLoad(llvm_intptr_type, entry_address_ptr, get_llvm_string_ref(u8"call_indirect.entry.addr"));
        context_address = ir_builder.CreateLoad(llvm_intptr_type, context_address_ptr, get_llvm_string_ref(u8"call_indirect.context.addr"));
        encoded_type_id = ir_builder.CreateLoad(llvm_i32_type, encoded_type_id_ptr, get_llvm_string_ref(u8"call_indirect.type.id"));
        typed_entry_address = ir_builder.CreateLoad(llvm_intptr_type, typed_entry_address_ptr, get_llvm_string_ref(u8"call_indirect.typed.entry.addr"));
        entry_address->setVolatile(true);
        context_address->setVolatile(true);
        typed_entry_address->setVolatile(true);
        entry_address->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
        context_address->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
        encoded_type_id->setAlignment(::llvm::Align{alignof(::std::uint_least32_t)});
        typed_entry_address->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
        if(state.lazy_defined_targets_are_atomic || state.debug_full_patchable_typed_target_base_address != 0u)
        {
            // Table views may be patched by lazy compilation.  Acquire ordering pairs with the runtime's publication of entry
            // and context addresses.
            entry_address->setAtomic(::llvm::AtomicOrdering::Acquire);
            context_address->setAtomic(::llvm::AtomicOrdering::Acquire);
            typed_entry_address->setAtomic(::llvm::AtomicOrdering::Acquire);
        }

    }

    emit_llvm_conditional_trap(*llvm_module,
                               ir_builder,
                               ir_builder.CreateICmpEQ(entry_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)),
                               ::uwvm2::runtime::lib::llvm_jit_trap_kind::call_indirect_null_element);
    auto const type_mismatch{emit_runtime_call_type_mismatch(ir_builder, encoded_type_id,
        expected_type_id, expected_type_width)};
    if(type_mismatch == nullptr) [[unlikely]] { return false; }
    emit_llvm_conditional_trap(*llvm_module,
                               ir_builder,
                               type_mismatch,
                               ::uwvm2::runtime::lib::llvm_jit_trap_kind::call_indirect_type_mismatch);

    if constexpr(Tail)
    {
        if(abi_layout.result_count > 1uz)
        {
            // [surviving caller's tuple ...] signature checked before this branch.
            // [safe                       ] forwarding excludes the retiring frame.
            prepared_call.arguments.push_back(state.llvm_function->getArg(state.llvm_function->arg_size() - 1uz));
        }
        if(state.llvm_function->getCallingConv() != ::llvm::CallingConv::Tail ||
           get_llvm_jit_typed_calling_conv(*typed_entry_function_type) != ::llvm::CallingConv::Tail ||
           typed_entry_function_type->getReturnType() != state.llvm_function->getReturnType()) { return false; }
        // A published typed entry is already callable. Missing entries may be
        // lazy Wasm functions or host functions; only the latter use an adapter.
        auto const fast{ir_builder.GetInsertBlock()};
        auto const slow{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"tail.indirect.resolve"), state.llvm_function)};
        auto const ready{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"tail.indirect.ready"), state.llvm_function)};
        ir_builder.CreateCondBr(ir_builder.CreateICmpEQ(typed_entry_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)), slow, ready);
        ir_builder.SetInsertPoint(slow);
        ::llvm::Value* resolved{::llvm::ConstantInt::get(llvm_intptr_type, 0u)};
        if(!state.emit_debug_safe_points)
        {
            auto const module_name{get_llvm_runtime_module_object_symbol_name(*runtime_module_ptr)};
            auto const module_address{get_llvm_external_host_object_address(ir_builder,
                reinterpret_cast<::std::uintptr_t>(runtime_module_ptr), ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()})};
            if(module_address == nullptr) { return false; }
            auto const resolver_type{::llvm::FunctionType::get(llvm_intptr_type, {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type}, false)};
            resolved = emit_runtime_local_func_llvm_jit_runtime_bridge_call<
                ::uwvm2::runtime::lib::details::llvm_jit_resolve_indirect_tail_target_abi_bridge>(state, resolver_type,
                    {module_address, ::llvm::ConstantInt::get(llvm_intptr_type, table_index), selector_index});
        }
        // Debug-full uses the copied target record from its live resolver.
        // Re-reading the table here could observe a later table.set with a
        // different ABI after the original signature check.
        if(resolved == nullptr) { return false; }
        auto const slow_end{ir_builder.GetInsertBlock()};
        ir_builder.CreateBr(ready);
        ir_builder.SetInsertPoint(ready);
        auto const address{ir_builder.CreatePHI(llvm_intptr_type, 2u, get_llvm_string_ref(u8"tail.indirect.address"))};
        address->addIncoming(typed_entry_address, fast); address->addIncoming(resolved, slow_end);
        auto const typed_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"tail.indirect.typed"), state.llvm_function)};
        auto const host_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"tail.indirect.host"), state.llvm_function)};
        // Retire only after all bounds/null/type checks and materialization;
        // a failure at this instruction must still report the current frame.
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        if(!sealed_compact_codegen::retire(state)) { return false; }
#endif
        if(!leave_runtime_llvm_jit_gc_root_frame(ir_builder, state.gc_root_frame)) { return false; }
        if(state.emit_call_stack_frames && !emit_runtime_local_func_llvm_jit_call_stack_pop(ir_builder)) { return false; }
        ir_builder.CreateCondBr(ir_builder.CreateICmpEQ(address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)), host_block, typed_block);
        ir_builder.SetInsertPoint(typed_block);
        if(!emit_runtime_local_func_llvm_jit_debug_activation_leave(ir_builder, state, 1u)) { return false; }
        // [published native typed target] ABI checked above, signature checked
        // [safe                         ] by the selected table's canonical id.
        auto const entry{ir_builder.CreateIntToPtr(address, get_llvm_pointer_type(typed_entry_function_type))};
        auto const transfer{apply_llvm_jit_wasm_calling_conv(ir_builder.CreateCall(typed_entry_function_type, entry,
            {prepared_call.arguments.data(), prepared_call.arguments.size()}))};
        transfer->setTailCallKind(::llvm::CallInst::TCK_MustTail);
        if(typed_entry_function_type->getReturnType()->isVoidTy()) { ir_builder.CreateRetVoid(); }
        else { ir_builder.CreateRet(transfer); }
        ir_builder.SetInsertPoint(host_block);
        if(!emit_runtime_local_func_llvm_jit_debug_activation_leave(ir_builder, state, 2u)) { return false; }
        auto const adapter{create_llvm_jit_host_tail_adapter(state, *callee_type_ptr)};
        if(adapter == nullptr) { return false; }
        ::uwvm2::utils::container::vector<::llvm::Value*> arguments{entry_address, context_address};
        for(auto argument: prepared_call.arguments) { arguments.push_back(argument); }
        auto const host_transfer{apply_llvm_jit_wasm_calling_conv(ir_builder.CreateCall(adapter, {arguments.data(), arguments.size()}))};
        host_transfer->setTailCallKind(::llvm::CallInst::TCK_MustTail);
        if(typed_entry_function_type->getReturnType()->isVoidTy()) { ir_builder.CreateRetVoid(); }
        else { ir_builder.CreateRet(host_transfer); }
        enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
        return true;
    }

    // The selector and arguments have been physically consumed. Publish the
    // exact waiting parent only after bounds/null/canonical-type checks succeed,
    // and before either real target enters. The restored edge uses the already
    // authenticated prepared child owner; it never reloads a mutable table.
    if(!emit_runtime_local_func_llvm_jit_checkpoint_awaiting_direct_call(state, *callee_type_ptr)) { return false; }

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
    auto typed_call{emit_llvm_jit_typed_wasm_call(ir_builder, *callee_type_ptr,
        typed_entry_function_type, typed_entry_function_ptr, {prepared_call.arguments.data(), prepared_call.arguments.size()},
            [&](::llvm::FunctionType* type, ::llvm::Value* callee, ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept
            { return emit_runtime_local_func_llvm_jit_may_throw_call(state, type, callee, arguments); })};
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
        [&](llvm_jit_runtime_raw_call_buffers_t const& raw_call_buffers) constexpr noexcept -> ::llvm::CallBase*
        {
            if(state.debug_activation_enabled)
            {
                return emit_runtime_local_func_llvm_jit_debug_raw_target_call(state, entry_address,
                    {context_address, raw_call_buffers.result_buffer_address,
                     ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.result_bytes), raw_call_buffers.param_buffer_address,
                     ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.parameter_bytes)});
            }
            auto raw_entry_function_ptr{
                ir_builder.CreateIntToPtr(entry_address, get_llvm_pointer_type(raw_entry_function_type), get_llvm_string_ref(u8"call_indirect.entry.ptr"))};
            return apply_llvm_jit_raw_entry_calling_conv(emit_runtime_local_func_llvm_jit_may_throw_call(state, raw_entry_function_type,
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

// Core 3 call_ref receives an opaque native funcref object after its arguments. The runtime bridge authenticates
// the VM-owned function record, copies one complete raw/typed target, and canonicalizes its signature; generated
// code never treats a guest-provided payload as an executable address. This path is emitted only for call_ref opcodes.
template <bool Tail = false>
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_call_ref(
    runtime_local_func_llvm_jit_emit_state_t& state, validation_module_traits_t::wasm_u32 type_index) noexcept
{
    if(!state.valid || state.local_func_storage_ptr == nullptr || state.llvm_context_holder == nullptr ||
       state.llvm_module == nullptr || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    auto const* runtime_module{state.local_func_storage_ptr->runtime_module_ptr};
    if(runtime_module == nullptr) [[unlikely]] { return false; }
    auto const* callee_type{resolve_runtime_type_section_function_type(*runtime_module, type_index)};
    if(callee_type == nullptr) [[unlikely]] { return false; }
    auto const abi_layout{get_runtime_wasm_call_abi_layout(*callee_type)};
    if(!abi_layout.valid || abi_layout.parameter_count == (::std::numeric_limits<::std::size_t>::max)() ||
       state.operand_stack.size() < abi_layout.parameter_count + 1uz) [[unlikely]] { return false; }

    // [argument descriptors ...][reference] end: the size proof above makes back() live.
    // [safe                            ]; pop removes only the descriptor, preserving its owned LLVM SSA value.
    auto const reference{state.operand_stack.back()};
    state.operand_stack.pop_back();
    auto& context{*state.llvm_context_holder};
    auto& builder{*state.ir_builder};
    auto const ref_type{get_llvm_type_from_wasm_value_type(context, runtime_operand_stack_value_type::funcref)};
    if(reference.type != runtime_operand_stack_value_type::funcref || reference.value == nullptr ||
       reference.value->getType() != ref_type) [[unlikely]] { return false; }

    // A ref.func SSA value names a specific module-owned function record, so it cannot become null or
    // change identity between its production and consumption. The fused validator has already
    // proved defined-type matching; check the native carrier ABI before direct call lowering. That lowering
    // already provides debug-full acquire slots, lazy/tiered materialization, EH and native musttail.
    // Unknown/dynamic references (including locals, tables and select) retain the checked bridge below.
    if(reference.known_ref_func_index != (::std::numeric_limits<::std::size_t>::max)())
    {
        if(reference.known_ref_func_index > (::std::numeric_limits<validation_module_traits_t::wasm_u32>::max)())
            [[unlikely]] { return false; }
        auto const known_index{static_cast<validation_module_traits_t::wasm_u32>(reference.known_ref_func_index)};
        auto const* known_type{resolve_runtime_callee_function_type(*runtime_module, known_index)};
        if(known_type == nullptr || !runtime_wasm_function_types_equal(*known_type, *callee_type)) [[unlikely]]
        { return false; }
        if constexpr(Tail) { return try_emit_runtime_local_func_llvm_jit_return_call(state, known_index); }
        else { return try_emit_runtime_local_func_llvm_jit_call(state, known_index); }
    }

    auto prepared{prepare_runtime_local_func_llvm_jit_wasm_call_operands(state, *callee_type, {::std::addressof(reference), 1uz})};
    if(!prepared.valid) [[unlikely]] { return false; }

    auto const intptr{::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const i32{::llvm::Type::getInt32Ty(context)};
    auto const raw_type{get_llvm_runtime_raw_call_target_struct_type(context)};
    auto const raw_entry_type{get_llvm_runtime_raw_call_target_entry_function_type(context)};
    auto const typed_type{get_llvm_function_type_from_wasm_function_type(context, *callee_type)};
    if(raw_type == nullptr || raw_entry_type == nullptr || typed_type == nullptr) [[unlikely]] { return false; }
    auto const expected_id{resolve_runtime_canonical_type_id(*runtime_module, type_index)};
    if(expected_id == invalid_runtime_canonical_type_id()) [[unlikely]] { return false; }
    auto const expected_width{resolve_runtime_call_type_match_width(*runtime_module, type_index)};
    if(expected_width == 0u) [[unlikely]] { return false; }

    auto const ref_slot{create_llvm_jit_entry_block_alloca(builder, ref_type, nullptr, get_llvm_string_ref(u8"call_ref.value"))};
    auto const target_slot{create_llvm_jit_entry_block_alloca(builder, raw_type, nullptr, get_llvm_string_ref(u8"call_ref.target"))};
    if(ref_slot == nullptr || target_slot == nullptr) [[unlikely]] { return false; }
    ref_slot->setAlignment(::llvm::Align{alignof(runtime_wasm_funcref)});
    target_slot->setAlignment(::llvm::Align{alignof(::uwvm2::uwvm::runtime::storage::llvm_jit_raw_call_target_t)});
    auto const ref_store{builder.CreateStore(reference.value, ref_slot)};
    ref_store->setAlignment(::llvm::Align{alignof(runtime_wasm_funcref)});
    auto const module_name{get_llvm_runtime_module_object_symbol_name(*runtime_module)};
    auto const module_address{get_llvm_external_host_object_address(builder,
        reinterpret_cast<::std::uintptr_t>(runtime_module),
        ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()})};
    if(module_address == nullptr) [[unlikely]] { return false; }
    // Both addresses name aligned host-owned stack slots. The bridge checks the caller module and
    // reference kind/pointer identity before copying a complete target; neither is a guest-memory address.
    auto const ref_address{builder.CreatePtrToInt(ref_slot, intptr, get_llvm_string_ref(u8"call_ref.value.address"))};
    auto const target_address{builder.CreatePtrToInt(target_slot, intptr, get_llvm_string_ref(u8"call_ref.target.address"))};
    auto const resolver_type{::llvm::FunctionType::get(intptr, {intptr, intptr, intptr}, false)};
    auto const status{emit_runtime_local_func_llvm_jit_runtime_bridge_call<
        ::uwvm2::runtime::lib::details::llvm_jit_resolve_call_ref_target_abi_bridge>(
            state, resolver_type, {module_address, ref_address, target_address})};
    if(status == nullptr) [[unlikely]] { return false; }
    emit_llvm_conditional_trap(*state.llvm_module, builder,
        builder.CreateICmpNE(status, ::llvm::ConstantInt::get(intptr, 0u)),
        ::uwvm2::runtime::lib::llvm_jit_trap_kind::null_reference);

    // The successful bridge wrote all fields of the aligned alloca before returning. Runtime mutation
    // cannot change this copy, so signature, entry and context are one coherent call target.
    auto const entry_address{builder.CreateLoad(intptr, builder.CreateStructGEP(raw_type, target_slot, 0u),
        get_llvm_string_ref(u8"call_ref.entry"))};
    auto const context_address{builder.CreateLoad(intptr, builder.CreateStructGEP(raw_type, target_slot, 1u),
        get_llvm_string_ref(u8"call_ref.context"))};
    auto const encoded_type_id{builder.CreateLoad(i32, builder.CreateStructGEP(raw_type, target_slot, 2u),
        get_llvm_string_ref(u8"call_ref.type.id"))};
    auto const typed_entry_address{builder.CreateLoad(intptr, builder.CreateStructGEP(raw_type, target_slot, 3u),
        get_llvm_string_ref(u8"call_ref.typed.entry"))};
    emit_llvm_conditional_trap(*state.llvm_module, builder,
        builder.CreateICmpEQ(entry_address, ::llvm::ConstantInt::get(intptr, 0u)),
        ::uwvm2::runtime::lib::llvm_jit_trap_kind::null_reference);
    auto const type_mismatch{emit_runtime_call_type_mismatch(builder, encoded_type_id, expected_id, expected_width)};
    if(type_mismatch == nullptr) [[unlikely]] { return false; }
    emit_llvm_conditional_trap(*state.llvm_module, builder,
        type_mismatch,
        ::uwvm2::runtime::lib::llvm_jit_trap_kind::call_indirect_type_mismatch);

    if constexpr(Tail)
    {
        if(state.llvm_function == nullptr) [[unlikely]] { return false; }
        if(abi_layout.result_count > 1uz)
        {
            // [surviving caller result tuple] Wasm result equivalence was checked by the scanner.
            // [safe                         ] forward its output slot; no retiring-frame buffer escapes.
            prepared.arguments.push_back(state.llvm_function->getArg(state.llvm_function->arg_size() - 1uz));
        }
        if(state.llvm_function->getCallingConv() != ::llvm::CallingConv::Tail ||
           get_llvm_jit_typed_calling_conv(*typed_type) != ::llvm::CallingConv::Tail ||
           typed_type->getReturnType() != state.llvm_function->getReturnType()) { return false; }
        auto const fast{builder.GetInsertBlock()};
        auto const slow{::llvm::BasicBlock::Create(context, get_llvm_string_ref(u8"tail.call_ref.resolve"), state.llvm_function)};
        auto const ready{::llvm::BasicBlock::Create(context, get_llvm_string_ref(u8"tail.call_ref.ready"), state.llvm_function)};
        builder.CreateCondBr(builder.CreateICmpEQ(typed_entry_address, ::llvm::ConstantInt::get(intptr, 0u)), slow, ready);
        builder.SetInsertPoint(slow);
        // A missing typed entry may be a lazy/tiered defined function. Materialize it before retiring
        // this frame; zero is reserved for a true host leaf handled by the owning tail adapter.
        auto const tail_resolver_type{::llvm::FunctionType::get(intptr, {intptr, intptr}, false)};
        auto const resolved{emit_runtime_local_func_llvm_jit_runtime_bridge_call<
            ::uwvm2::runtime::lib::details::llvm_jit_resolve_call_ref_tail_target_abi_bridge>(
                state, tail_resolver_type, {module_address, ref_address})};
        if(resolved == nullptr) [[unlikely]] { return false; }
        auto const slow_end{builder.GetInsertBlock()};
        builder.CreateBr(ready);
        builder.SetInsertPoint(ready);
        auto const address{builder.CreatePHI(intptr, 2u, get_llvm_string_ref(u8"tail.call_ref.address"))};
        address->addIncoming(typed_entry_address, fast);
        address->addIncoming(resolved, slow_end);
        auto const typed_block{::llvm::BasicBlock::Create(context, get_llvm_string_ref(u8"tail.call_ref.typed"), state.llvm_function)};
        auto const host_block{::llvm::BasicBlock::Create(context, get_llvm_string_ref(u8"tail.call_ref.host"), state.llvm_function)};
        // Trap and materialization must observe the current logical frame; retire it only after success.
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        if(!sealed_compact_codegen::retire(state)) { return false; }
#endif
        if(!leave_runtime_llvm_jit_gc_root_frame(builder, state.gc_root_frame)) { return false; }
        if(state.emit_call_stack_frames && !emit_runtime_local_func_llvm_jit_call_stack_pop(builder)) { return false; }
        builder.CreateCondBr(builder.CreateICmpEQ(address, ::llvm::ConstantInt::get(intptr, 0u)), host_block, typed_block);
        builder.SetInsertPoint(typed_block);
        if(!emit_runtime_local_func_llvm_jit_debug_activation_leave(builder, state, 1u)) { return false; }
        auto const typed_entry{builder.CreateIntToPtr(address, get_llvm_pointer_type(typed_type))};
        auto const transfer{apply_llvm_jit_wasm_calling_conv(builder.CreateCall(typed_type, typed_entry,
            {prepared.arguments.data(), prepared.arguments.size()}))};
        transfer->setTailCallKind(::llvm::CallInst::TCK_MustTail);
        if(typed_type->getReturnType()->isVoidTy()) { builder.CreateRetVoid(); }
        else { builder.CreateRet(transfer); }
        builder.SetInsertPoint(host_block);
        if(!emit_runtime_local_func_llvm_jit_debug_activation_leave(builder, state, 2u)) { return false; }
        auto const adapter{create_llvm_jit_host_tail_adapter(state, *callee_type)};
        if(adapter == nullptr) [[unlikely]] { return false; }
        ::uwvm2::utils::container::vector<::llvm::Value*> host_arguments{entry_address, context_address};
        for(auto argument: prepared.arguments) { host_arguments.push_back(argument); }
        auto const host_transfer{apply_llvm_jit_wasm_calling_conv(builder.CreateCall(adapter,
            {host_arguments.data(), host_arguments.size()}))};
        host_transfer->setTailCallKind(::llvm::CallInst::TCK_MustTail);
        if(typed_type->getReturnType()->isVoidTy()) { builder.CreateRetVoid(); }
        else { builder.CreateRet(host_transfer); }
        enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
        return true;
    }

    // The VM-owned resolver copied one coherent target and canonical matching
    // succeeded above. Only now may this exact consumed-prefix become the
    // waiting caller packet. Restore invokes the captured actual child owner,
    // without reloading the original reference or accepting an executable token.
    if(!emit_runtime_local_func_llvm_jit_checkpoint_awaiting_direct_call(state, *callee_type)) { return false; }

    auto const current{builder.GetInsertBlock()};
    if(current == nullptr || current->getParent() == nullptr) [[unlikely]] { return false; }
    auto const typed_block{::llvm::BasicBlock::Create(context, get_llvm_string_ref(u8"call_ref.typed"), current->getParent())};
    auto const raw_block{::llvm::BasicBlock::Create(context, get_llvm_string_ref(u8"call_ref.raw"), current->getParent())};
    auto const merge_block{::llvm::BasicBlock::Create(context, get_llvm_string_ref(u8"call_ref.merge"), current->getParent())};
    builder.CreateCondBr(builder.CreateICmpNE(typed_entry_address, ::llvm::ConstantInt::get(intptr, 0u)), typed_block, raw_block);
    builder.SetInsertPoint(typed_block);
    auto const typed_entry{builder.CreateIntToPtr(typed_entry_address, get_llvm_pointer_type(typed_type))};
    auto const typed_call{emit_llvm_jit_typed_wasm_call(builder, *callee_type, typed_type, typed_entry,
        {prepared.arguments.data(), prepared.arguments.size()},
        [&](::llvm::FunctionType* type, ::llvm::Value* callee, ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept
        { return emit_runtime_local_func_llvm_jit_may_throw_call(state, type, callee, arguments); })};
    if(typed_call == nullptr) [[unlikely]] { return false; }
    auto const typed_end{builder.GetInsertBlock()};
    builder.CreateBr(merge_block);
    builder.SetInsertPoint(raw_block);
    auto const raw_result{emit_runtime_local_func_llvm_jit_runtime_raw_host_bridge_call(state, *callee_type,
        {prepared.arguments.data(), prepared.arguments.size()},
        get_llvm_string_ref(u8"call_ref.params"), get_llvm_string_ref(u8"call_ref.result.buf"),
        [&](llvm_jit_runtime_raw_call_buffers_t const& buffers) constexpr noexcept -> ::llvm::CallBase*
        {
            if(state.debug_activation_enabled)
            {
                return emit_runtime_local_func_llvm_jit_debug_raw_target_call(state, entry_address,
                    {context_address, buffers.result_buffer_address, ::llvm::ConstantInt::get(intptr, abi_layout.result_bytes),
                     buffers.param_buffer_address, ::llvm::ConstantInt::get(intptr, abi_layout.parameter_bytes)});
            }
            auto const raw_entry{builder.CreateIntToPtr(entry_address, get_llvm_pointer_type(raw_entry_type))};
            return apply_llvm_jit_raw_entry_calling_conv(emit_runtime_local_func_llvm_jit_may_throw_call(
                state, raw_entry_type, raw_entry,
                {context_address, buffers.result_buffer_address,
                 ::llvm::ConstantInt::get(intptr, abi_layout.result_bytes),
                 buffers.param_buffer_address, ::llvm::ConstantInt::get(intptr, abi_layout.parameter_bytes)}));
        })};
    if(!raw_result.valid) [[unlikely]] { return false; }
    auto const raw_end{builder.GetInsertBlock()};
    builder.CreateBr(merge_block);
    builder.SetInsertPoint(merge_block);
    ::llvm::Value* result{};
    if(get_runtime_block_result_count(prepared.results) != 0uz)
    {
        auto const result_type{get_llvm_result_type_from_wasm_result_range(context, prepared.results.begin, prepared.results.end)};
        if(result_type == nullptr || typed_call->getType() != result_type ||
           raw_result.result_value == nullptr || raw_result.result_value->getType() != result_type) [[unlikely]] { return false; }
        auto const phi{builder.CreatePHI(result_type, 2u, get_llvm_string_ref(u8"call_ref.result"))};
        phi->addIncoming(typed_call, typed_end);
        phi->addIncoming(raw_result.result_value, raw_end);
        result = phi;
    }
    return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared, result);
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

#include "single_func_i32_numeric_event_emit.h"
#include "single_func_i64_numeric_event_emit.h"
#include "single_func_integer_width_event_emit.h"
#include "single_func_integer_compare_event_emit.h"
#include "single_func_table_access_event_emit.h"

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

#include "memory_emit.h"
#include "simd_emit.h"

// SIMD arithmetic is lowered to target-independent vector IR. Buffers are reserved for provider/pinned-allocation
// fallbacks; native mmap SIMD shares exactly the scalar address/protection path.
template <llvm_jit_simd_code Op,
          ::uwvm2::runtime::compiler::shared::wasm1p1_simd_instruction_kind Kind,
          ::uwvm2::runtime::compiler::shared::wasm1p1_simd_scalar_kind ScalarKind,
          ::std::size_t LaneCount,
          ::std::uint_least32_t MaxAlign>
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_typed_simd_instruction(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::std::uint64_t static_offset,
    runtime_wasm_u32 lane,
    ::std::byte const* immediate_bytes) noexcept
{
    using simd_kind = ::uwvm2::runtime::compiler::shared::wasm1p1_simd_instruction_kind;
    using value_type = runtime_operand_stack_value_type;
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_module == nullptr ||
       state.ir_builder == nullptr || state.local_func_storage_ptr == nullptr) { return false; }
    auto& ir_builder{*state.ir_builder};
    auto& operand_stack{state.operand_stack};

    if constexpr(Kind != simd_kind::memory_load && Kind != simd_kind::memory_store)
    {
        ::llvm::Value *a{}, *b{}, *c{};
        auto const pop{[&](value_type expected) noexcept -> ::llvm::Value*
        {
            if(operand_stack.empty()) { return nullptr; }
            auto v{operand_stack.back()};
            operand_stack.pop_back();
            return v.type == expected ? v.value : nullptr;
        }};
        if constexpr(Kind == simd_kind::constant) {}
        else if constexpr(Kind == simd_kind::splat)
        {
            a = pop(llvm_jit_simd_scalar_traits<ScalarKind>::value_type);
            if(a == nullptr) { return false; }
        }
        else
        {
            if constexpr(Kind == simd_kind::ternary)
            {
                c = pop(value_type::v128);
                if(c == nullptr) { return false; }
            }
            if constexpr(Kind == simd_kind::binary || Kind == simd_kind::shuffle || Kind == simd_kind::ternary)
            { b = pop(value_type::v128); if(b == nullptr) { return false; } }
            else if constexpr(Kind == simd_kind::shift)
            { b = pop(value_type::i32); if(b == nullptr) { return false; } }
            else if constexpr(Kind == simd_kind::replace_lane)
            { b = pop(llvm_jit_simd_scalar_traits<ScalarKind>::value_type); if(b == nullptr) { return false; } }
            a = pop(value_type::v128);
            if(a == nullptr) { return false; }
        }
        auto value{simd_ir::emit_value(ir_builder, Op, a, b, c, lane, immediate_bytes)};
        if(value == nullptr) { return false; }
        constexpr auto result_type{[]() constexpr noexcept
        {
            if constexpr(Kind == simd_kind::test) { return value_type::i32; }
            else if constexpr(Kind == simd_kind::extract_lane) { return llvm_jit_simd_scalar_traits<ScalarKind>::value_type; }
            else { return value_type::v128; }
        }()};
        operand_stack.push_back({.type = result_type, .value = value});
        return true;
    }
    else
    {
        auto& llvm_context{*state.llvm_context_holder};
        auto llvm_v128_type{get_llvm_type_from_wasm_value_type(llvm_context, value_type::v128)};
        auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
        auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
        auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
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

        auto const& local_func_storage{*state.local_func_storage_ptr};
        auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
        if(runtime_module_ptr == nullptr) [[unlikely]] { return false; }
        if(!state.selected_memory_access_info_resolved)
        {
            state.selected_memory_access_info = resolve_runtime_memory_access_info(*runtime_module_ptr, state.current_memory_index);
            state.selected_memory_access_info_resolved = true;
        }
        auto const& memory_info{state.selected_memory_access_info};
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
        bool const address64{::uwvm2::uwvm::runtime::storage::runtime_memory_is_address64(*runtime_module_ptr, state.current_memory_index)};
        if(!address64 && static_offset > ::std::numeric_limits<runtime_wasm_u32>::max()) { return false; }
        if(address.type != (address64 ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32) || address.value == nullptr) [[unlikely]] { return false; }

        constexpr auto access_size{llvm_jit_simd_details::simd_memory_access_size<Op>()};
        if(auto pointer{emit_llvm_jit_direct_memory_pointer(state, static_offset, access_size, address.value, Kind == simd_kind::memory_store)}; pointer != nullptr)
        {
            if constexpr(Kind == simd_kind::memory_load)
            {
                auto value{simd_ir::emit_load<Op>(ir_builder, pointer, vector_value.value, lane,
                    memory_info.direct_memory_is_shared || !llvm_jit_memory_span_within_declared_minimum(
                        memory_info, address.value, static_offset, access_size))};
                if(value == nullptr) { return false; }
                operand_stack.push_back({.type = runtime_operand_stack_value_type::v128, .value = value});
                return true;
            }
            else { return simd_ir::emit_store<Op>(ir_builder, pointer, vector_value.value, lane) != nullptr; }
        }

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
            auto const symbol_name{get_llvm_native_memory_object_symbol_name(*runtime_module_ptr, state.current_memory_index)};
            auto memory_address{get_llvm_external_host_object_address(
                ir_builder,
                reinterpret_cast<::std::uintptr_t>(memory_info.memory_p),
                ::uwvm2::utils::container::u8string_view{symbol_name.data(), symbol_name.size()})};
            if(memory_address == nullptr) [[unlikely]] { return false; }
            if(address64)
            {
                // Reuse the qualified wide ABI; buffers are owned native stack slots,
                // never guest addresses, and the bridge checks the entire u65 span.
                call = emit_llvm_jit_memory64_simd_call<Op>(ir_builder, memory_address,
                    ir_builder.getInt64(static_offset), address.value, vector_address, lane, result_address);
                if(call == nullptr) { return false; }
                if constexpr(Kind == simd_kind::memory_load) { return push_v128_result(result_buffer); }
                else { return true; }
            }
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
            if(address64) { return false; }
            auto const symbol_name{get_llvm_local_imported_memory_module_symbol_name(
                *runtime_module_ptr,
                state.current_memory_index)};
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
}

#include "single_func_gc_emit.h"
#include "single_func_gc_normalized_emit.h"
#include "single_func_ref_null_event_emit.h"
#include "single_func_ref_is_null_event_emit.h"
#include "single_func_ref_func_event_emit.h"
#include "single_func_simd_event_emit.h"

// Typed scalar/page specializations emit from checked compiler DATA only.
// The legacy specialization remains a raw replay for other, explicitly pending opcode families.
template<bool NormalizedScalarMemory, bool NormalizedPageMemory = false, bool NormalizedSimd = false, bool NormalizedAtomic = false, bool NormalizedBulkMemory = false, bool NormalizedI32Numeric = false, bool NormalizedI64Numeric = false, bool NormalizedIntegerWidth = false, bool NormalizedIntegerCompare = false, bool NormalizedTableAccess = false>
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_instruction_impl(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                     ::std::byte const* instruction_begin,
                                                                                     ::std::byte const* instruction_end,
                                                                                     ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event const* memory_event,
                                                                                     ::uwvm2::validation::standard::wasm3::validated_memory_page_event const* page_event = nullptr,
                                                                                     ::uwvm2::validation::standard::wasm3::validated_simd_event const* simd_event = nullptr,
                                                                                     ::uwvm2::validation::standard::wasm3::validated_atomic_event const* atomic_event = nullptr,
                                                                                     ::uwvm2::validation::standard::wasm3::validated_bulk_memory_event const* bulk_event = nullptr,
                                                                                     ::uwvm2::validation::standard::wasm3::validated_i32_numeric_event const* numeric_event = nullptr,
                                                                                     ::uwvm2::validation::standard::wasm3::validated_i64_numeric_event const* numeric64_event = nullptr,
                                                                                     ::uwvm2::validation::standard::wasm3::validated_integer_width_event const* width_event = nullptr,
                                                                                     ::uwvm2::validation::standard::wasm3::validated_integer_compare_event const* compare_event = nullptr,
                                                                                     ::uwvm2::validation::standard::wasm3::validated_table_access_event const* table_event = nullptr) noexcept
{
    static_assert(static_cast<unsigned>(NormalizedScalarMemory) + static_cast<unsigned>(NormalizedPageMemory) +
                  static_cast<unsigned>(NormalizedSimd) + static_cast<unsigned>(NormalizedAtomic) + static_cast<unsigned>(NormalizedBulkMemory) + static_cast<unsigned>(NormalizedI32Numeric) + static_cast<unsigned>(NormalizedI64Numeric) + static_cast<unsigned>(NormalizedIntegerWidth) + static_cast<unsigned>(NormalizedIntegerCompare) + static_cast<unsigned>(NormalizedTableAccess) <= 1u);
    if(!state.valid || state.local_func_storage_ptr == nullptr || state.llvm_context_holder == nullptr || state.llvm_module == nullptr ||
       state.llvm_function == nullptr || state.ir_builder == nullptr) [[unlikely]]
    {
        return false;
    }

    if constexpr(NormalizedScalarMemory)
    {
        // Compiler-owned checked DATA only. This entry never borrows a raw instruction slice.
        if(memory_event == nullptr) { return false; }
    }
    else if constexpr(NormalizedPageMemory)
    {
        // The typed producer owns DATA only, never a raw expression slice.
        if(page_event == nullptr) { return false; }
    }
    else if constexpr(NormalizedSimd)
    {
        if(simd_event == nullptr) { return false; }
    }
    else if constexpr(NormalizedAtomic)
    {
        if(atomic_event == nullptr) { return false; }
    }
    else if constexpr(NormalizedBulkMemory)
    {
        if(bulk_event == nullptr) { return false; }
    }
    else if constexpr(NormalizedI32Numeric)
    {
        if(numeric_event == nullptr) { return false; }
    }
    else if constexpr(NormalizedI64Numeric)
    {
        if(numeric64_event == nullptr) { return false; }
    }
    else if constexpr(NormalizedIntegerWidth)
    {
        if(width_event == nullptr) { return false; }
    }
    else if constexpr(NormalizedIntegerCompare)
    {
        if(compare_event == nullptr) { return false; }
    }
    else if constexpr(NormalizedTableAccess)
    {
        if(table_event == nullptr) { return false; }
    }
    else
    {
        // [original validated instruction_begin ... instruction_end] | one-past
        // [safe only for a nonempty original slice                 ] | never read here.
        if(instruction_begin == nullptr || instruction_end == nullptr || instruction_begin >= instruction_end) { return false; }
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
    auto& selected_memory_access_info{state.selected_memory_access_info};
    auto& selected_memory_access_info_resolved{state.selected_memory_access_info_resolved};
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

    // The instruction's decoded index owns this cache. Changing memories invalidates the resolved
    // object, provider and limits together; compile-time selection adds no runtime index dispatch.
    auto const select_memory{[&](validation_module_traits_t::wasm_u32 index) constexpr noexcept
    {
        if(state.current_memory_index != index)
        {
            state.current_memory_index = index;
            selected_memory_access_info = {};
            selected_memory_access_info_resolved = false;
        }
    }};
    [[maybe_unused]] auto const parse_selected_memory_index{[&]() constexpr noexcept -> bool
    {
        validation_module_traits_t::wasm_u32 index{};
        // [opcode] memidx ... (code_end); replay receives a validated instruction range.
        if(::uwvm2::validation::standard::wasm3::scan_memory_index(code_curr, code_end, true, index) !=
           ::uwvm2::validation::standard::wasm3::memory_immediate_error::ok) { return false; }
        // [opcode memidx] ... unsafe (could be code_end); code_curr is after the bounded u32.
        select_memory(index);
        return true;
    }};
    auto const selected_memory_is_address64{[&]() constexpr noexcept -> bool
    {
        auto const module{local_func_storage.runtime_module_ptr}; // Borrow the translation-owned module record.
        if(module == nullptr) { return false; }
        return ::uwvm2::uwvm::runtime::storage::runtime_memory_is_address64(*module, state.current_memory_index);
    }};

    auto const ensure_selected_memory_access_info{[&]() constexpr noexcept
                                          {
                                              if(selected_memory_access_info_resolved)
                                              {
                                                  return selected_memory_access_info.memory_p != nullptr || selected_memory_access_info.local_imported_module_ptr != nullptr;
                                              }

                                              auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
                                              if(runtime_module_ptr == nullptr) [[unlikely]]
                                              {
                                                  selected_memory_access_info_resolved = true;
                                                  return false;
                                              }

                                              selected_memory_access_info = resolve_runtime_memory_access_info(*runtime_module_ptr, state.current_memory_index);
                                              selected_memory_access_info_resolved = true;
                                              return selected_memory_access_info.memory_p != nullptr || selected_memory_access_info.local_imported_module_ptr != nullptr;
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
            if(selected_memory_access_info.local_imported_module_ptr == nullptr) [[unlikely]] { return nullptr; }

            auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
            if(runtime_module_ptr == nullptr) [[unlikely]] { return nullptr; }

            auto const module_symbol_name{get_llvm_local_imported_memory_module_symbol_name(
                *runtime_module_ptr, state.current_memory_index)};
            return get_llvm_external_host_object_address(ir_builder,
                                                         reinterpret_cast<::std::uintptr_t>(selected_memory_access_info.local_imported_module_ptr),
                                                         ::uwvm2::utils::container::u8string_view{module_symbol_name.data(), module_symbol_name.size()});
        }};

    auto const emit_native_memory_object_address{[&]() constexpr noexcept -> ::llvm::Value*
                                                 {
                                                     if(selected_memory_access_info.memory_p == nullptr) [[unlikely]] { return nullptr; }

                                                     auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
                                                     if(runtime_module_ptr == nullptr) [[unlikely]] { return nullptr; }

                                                     auto const memory_symbol_name{get_llvm_native_memory_object_symbol_name(*runtime_module_ptr, state.current_memory_index)};
                                                     return get_llvm_external_host_object_address(
                                                         ir_builder,
                                                         reinterpret_cast<::std::uintptr_t>(selected_memory_access_info.memory_p),
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
                    if(static_cast<unsigned>(table->table_type_ptr->reftype) == 0x69u)
                    {
                        result_type = static_cast<runtime_operand_stack_value_type>(0x69u);
                        return true;
                    }
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

    auto const emit_ref_as_non_null{[&]() constexpr noexcept -> bool
    {
        if(operand_stack.empty()) [[unlikely]] { return false; }
        // [owned SSA descriptors ...][reference] end
        // [safe                                ] empty() proved back() is available; no pointer/stack movement.
        auto const reference{operand_stack.back()};
        if((reference.type != runtime_operand_stack_value_type::funcref &&
            reference.type != runtime_operand_stack_value_type::externref &&
            get_runtime_wasm_value_type_encoding(reference.type) != 0x69u) || reference.value == nullptr) [[unlikely]]
        { return false; }
        auto const is_null{emit_llvm_jit_ref_is_null(ir_builder, reference.value, llvm_module->getDataLayout().isLittleEndian())};
        if(is_null == nullptr) [[unlikely]] { return false; }
        // A proven non-null tag needs no trap diamond, even in an unoptimized/lazy compilation tier.
        // Dynamic tags keep the existing explicit check; no payload value can establish this proof.
        if(auto const constant{::llvm::dyn_cast<::llvm::ConstantInt>(is_null)}; constant != nullptr && constant->isZero()) { return true; }
        auto const condition{ir_builder.CreateICmpNE(is_null, ::llvm::ConstantInt::get(is_null->getType(), 0u))};
        emit_llvm_conditional_trap(*llvm_module, ir_builder, condition, ::uwvm2::runtime::lib::llvm_jit_trap_kind::null_reference);
        return true;
    }};

    auto const table_address64_at{[&](validation_module_traits_t::wasm_u32 index) constexpr noexcept
    {
        return ::uwvm2::uwvm::runtime::storage::runtime_table_is_address64(*local_func_storage.runtime_module_ptr, index);
    }};
    auto const table_operand_type{[&](validation_module_traits_t::wasm_u32 index) constexpr noexcept
    { return table_address64_at(index) ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32; }};
    // Width selection happens while compiling the module; generated calls use one exact native ABI.
    // Materialize selected calls before testing for null: Clang 23 diagnoses a
    // direct comparison of an explicitly instantiated generic lambda call as
    // a constant function-pointer conversion instead of comparing CallInst*.
    auto const emit_table_width_bridge_call{[&]<auto Narrow, auto Wide>(bool wide, ::llvm::FunctionType* type,
        ::llvm::ArrayRef<::llvm::Value*> arguments) constexpr noexcept
    {
        // These width-selected table leaves access engine-owned table storage;
        // they never invoke an imported function or foreign provider callback.
        // Keep the real Wasm activation visible if a bounds check traps while
        // holding the table guard. The terminal trap observer releases that
        // guard before exposing the saved pre-op operands to the debugger.
        // Treating these internal leaves as foreign host islands hides genuine
        // Wasm traps and does not protect any actual host continuation.
        if(state.debug_activation_enabled)
        {
            auto const target{wide ? get_llvm_runtime_bridge_function_symbol_value_unwrapped<Wide>(*state.ir_builder, type) :
                get_llvm_runtime_bridge_function_symbol_value_unwrapped<Narrow>(*state.ir_builder, type)};
            if(target == nullptr) { return static_cast<::llvm::CallInst*>(nullptr); }
            return apply_llvm_jit_host_calling_conv(state.ir_builder->CreateCall(type, target, arguments));
        }
        if(wide) { return emit_runtime_local_func_llvm_jit_runtime_bridge_call<Wide>(state, type, arguments); }
        return emit_runtime_local_func_llvm_jit_runtime_bridge_call<Narrow>(state, type, arguments);
    }};

#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE == 1
    auto const emit_table_get_call_unsealed{[&](validation_module_traits_t::wasm_u32 table_index,
#else
    auto const emit_table_get_call{[&](validation_module_traits_t::wasm_u32 table_index,
#endif
                                       runtime_operand_stack_value_type reference_value_type) constexpr noexcept -> bool
                                   {
                                       if(operand_stack.empty()) [[unlikely]] { return false; }
                                       auto const index{operand_stack.back()};
                                       operand_stack.pop_back();
                                       if(index.type != table_operand_type(table_index) || index.value == nullptr) [[unlikely]] { return false; }

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
                                           {llvm_intptr_type, llvm_i32_type, index.value->getType(), llvm_intptr_type},
                                           false)};
                                       ::llvm::Value* bridge_arguments[]{
                                           module_address,
                                           ::llvm::ConstantInt::get(llvm_i32_type, table_index),
                                           index.value,
                                           result_address};
                                       auto const bridge_call{emit_table_width_bridge_call.template operator()<&llvm_jit_table_get_bridge<runtime_wasm_i32>, &llvm_jit_table_get_bridge<runtime_wasm_i64>>(table_address64_at(table_index), bridge_function_type, {bridge_arguments})};
                                       if(bridge_call == nullptr) [[unlikely]]
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

#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE == 1
    auto const emit_table_set_call_unsealed{[&](validation_module_traits_t::wasm_u32 table_index,
#else
    auto const emit_table_set_call{[&](validation_module_traits_t::wasm_u32 table_index,
#endif
                                       runtime_operand_stack_value_type reference_value_type) constexpr noexcept -> bool
                                   {
                                       if(operand_stack.size() < 2uz) [[unlikely]] { return false; }
                                       auto const value{operand_stack.back()};
                                       operand_stack.pop_back();
                                       auto const index{operand_stack.back()};
                                       operand_stack.pop_back();
                                       if(value.type != reference_value_type || value.value == nullptr ||
                                          index.type != table_operand_type(table_index) || index.value == nullptr) [[unlikely]]
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
                                           {llvm_intptr_type, llvm_i32_type, index.value->getType(), llvm_intptr_type},
                                           false)};
                                       ::llvm::Value* bridge_arguments[]{
                                           module_address,
                                           ::llvm::ConstantInt::get(llvm_i32_type, table_index),
                                           index.value,
                                           value_address};
                                       auto const bridge_call{emit_table_width_bridge_call.template operator()<&llvm_jit_table_set_bridge<runtime_wasm_i32>, &llvm_jit_table_set_bridge<runtime_wasm_i64>>(table_address64_at(table_index), bridge_function_type, {bridge_arguments})};
                                       return bridge_call != nullptr;
                                   }};

#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE == 1
    auto const emit_table_get_call{[&](validation_module_traits_t::wasm_u32 table_index,
        runtime_operand_stack_value_type reference_type) constexpr noexcept -> bool
    {
        return sealed_local_table_codegen::table_get(state, table_index, reference_type,
            table_address64_at(table_index), emit_table_get_call_unsealed);
    }};
    auto const emit_table_set_call{[&](validation_module_traits_t::wasm_u32 table_index,
        runtime_operand_stack_value_type reference_type) constexpr noexcept -> bool
    {
        return sealed_local_table_codegen::table_set(state, table_index, reference_type,
            table_address64_at(table_index), emit_table_set_call_unsealed);
    }};
#endif

    // Dispatcher-local local-imported global getter.  Some opcode case files use this directly instead of the standalone
    // helper when all required LLVM locals are already in scope.
    [[maybe_unused]] auto const emit_local_imported_global_get_bridge_call{
        [&](runtime_global_access_info_t const& global_access_info, ::llvm::Type* llvm_global_type) constexpr noexcept -> ::llvm::Value*
        {
            if(global_access_info.local_imported_module_ptr == nullptr || llvm_global_type == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
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
            auto global_index_value{::llvm::ConstantInt::get(llvm_intptr_type, global_access_info.local_imported_global_index)};

            switch(get_llvm_jit_local_imported_global_bridge_abi(global_access_info.value_type))
            {
                case llvm_jit_local_imported_global_bridge_abi::scalar_value:
                {
                    auto bridge_function_type{::llvm::FunctionType::get(get_llvm_jit_scalar_bits_type(llvm_global_type), {llvm_intptr_type, llvm_intptr_type}, false)};
                    ::llvm::Value* bridge_arguments_array[]{module_address, global_index_value};
                    auto result{emit_runtime_scalar_bridge_call.template operator()<llvm_jit_local_imported_global_get_bridge<runtime_wasm_i32>,
                                                                               llvm_jit_local_imported_global_get_bridge<runtime_wasm_i64>,
                                                                               llvm_jit_local_imported_global_get_bridge<runtime_wasm_i32>,
                                                                               llvm_jit_local_imported_global_get_bridge<runtime_wasm_i64>>(
                        global_access_info.value_type,
                        bridge_function_type,
                        ::llvm::ArrayRef<::llvm::Value*>{bridge_arguments_array})};
                    return result == nullptr ? nullptr : ir_builder.CreateBitCast(result, llvm_global_type);
                }
                case llvm_jit_local_imported_global_bridge_abi::byte_buffer:
                {
                    auto byte_buffer{create_llvm_jit_local_imported_global_byte_buffer(
                        ir_builder, global_access_info.value_type, llvm_global_type, get_llvm_string_ref(u8"global.local_imported.get.bytes"))};
                    if(byte_buffer == nullptr) [[unlikely]] { return nullptr; }
                    auto byte_buffer_address{
                        ir_builder.CreatePtrToInt(byte_buffer, llvm_intptr_type, get_llvm_string_ref(u8"global.local_imported.get.bytes.addr"))};
                    auto bridge_function_type{::llvm::FunctionType::get(
                        ::llvm::Type::getVoidTy(llvm_context), {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type}, false)};
                    ::llvm::Value* bridge_arguments_array[]{module_address, global_index_value, byte_buffer_address};
                    auto bridge_call{emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_global_get_byte_buffer_bridge>(
                        bridge_function_type, ::llvm::ArrayRef<::llvm::Value*>{bridge_arguments_array})};
                    if(bridge_call == nullptr) [[unlikely]]
                    {
                        return nullptr;
                    }
                    auto loaded_value{ir_builder.CreateLoad(llvm_global_type, byte_buffer, get_llvm_string_ref(u8"global.local_imported.get"))};
                    loaded_value->setAlignment(get_llvm_jit_local_imported_global_byte_buffer_alignment(global_access_info.value_type));
                    return loaded_value;
                }
                [[unlikely]] case llvm_jit_local_imported_global_bridge_abi::unsupported:
                    return nullptr;
            }
            return nullptr;
        }};

    // Dispatcher-local local-imported global setter matching the getter above.
    [[maybe_unused]] auto const emit_local_imported_global_set_bridge_call{
        [&](runtime_global_access_info_t const& global_access_info, ::llvm::Type* llvm_value_type, ::llvm::Value* value) constexpr noexcept -> ::llvm::CallInst*
        {
            if(global_access_info.local_imported_module_ptr == nullptr || llvm_value_type == nullptr || value == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
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
            auto global_index_value{::llvm::ConstantInt::get(llvm_intptr_type, global_access_info.local_imported_global_index)};

            switch(get_llvm_jit_local_imported_global_bridge_abi(global_access_info.value_type))
            {
                case llvm_jit_local_imported_global_bridge_abi::scalar_value:
                {
                    auto bridge_function_type{
                        ::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context), {llvm_intptr_type, llvm_intptr_type, get_llvm_jit_scalar_bits_type(llvm_value_type)}, false)};
                    ::llvm::Value* bridge_arguments_array[]{module_address, global_index_value, ir_builder.CreateBitCast(value, get_llvm_jit_scalar_bits_type(llvm_value_type))};
                    return emit_runtime_scalar_bridge_call.template operator()<llvm_jit_local_imported_global_set_bridge<runtime_wasm_i32>,
                                                                               llvm_jit_local_imported_global_set_bridge<runtime_wasm_i64>,
                                                                               llvm_jit_local_imported_global_set_bridge<runtime_wasm_i32>,
                                                                               llvm_jit_local_imported_global_set_bridge<runtime_wasm_i64>>(
                        global_access_info.value_type,
                        bridge_function_type,
                        ::llvm::ArrayRef<::llvm::Value*>{bridge_arguments_array});
                }
                case llvm_jit_local_imported_global_bridge_abi::byte_buffer:
                {
                    auto byte_buffer{create_llvm_jit_local_imported_global_byte_buffer(
                        ir_builder, global_access_info.value_type, llvm_value_type, get_llvm_string_ref(u8"global.local_imported.set.bytes"))};
                    if(byte_buffer == nullptr) [[unlikely]] { return nullptr; }
                    auto store{ir_builder.CreateStore(value, byte_buffer)};
                    store->setAlignment(get_llvm_jit_local_imported_global_byte_buffer_alignment(global_access_info.value_type));
                    auto byte_buffer_address{
                        ir_builder.CreatePtrToInt(byte_buffer, llvm_intptr_type, get_llvm_string_ref(u8"global.local_imported.set.bytes.addr"))};
                    auto bridge_function_type{::llvm::FunctionType::get(
                        ::llvm::Type::getVoidTy(llvm_context), {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type}, false)};
                    ::llvm::Value* bridge_arguments_array[]{module_address, global_index_value, byte_buffer_address};
                    return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_global_set_byte_buffer_bridge>(
                        bridge_function_type, ::llvm::ArrayRef<::llvm::Value*>{bridge_arguments_array});
                }
                [[unlikely]] case llvm_jit_local_imported_global_bridge_abi::unsupported:
                    return nullptr;
            }
            return nullptr;
        }};

    // Scalar and SIMD use the same grow-aware byte-length slot.
    auto const emit_direct_memory_byte_length_value{[&]() noexcept -> ::llvm::Value*
    {
        if(!ensure_selected_memory_access_info()) { return nullptr; }
        return emit_llvm_jit_memory_length(state);
    }};

    // Convert a byte length to the selected Wasm page-count type using either a shift for power-of-two page sizes or division for
    // custom page sizes.
    auto const emit_direct_memory_page_count_from_byte_length{
        [&](::llvm::Value* memory_length_load, ::std::size_t page_size_bytes) constexpr noexcept -> ::llvm::Value*
        {
            if(memory_length_load == nullptr || page_size_bytes == 0uz) [[unlikely]] { return nullptr; }

            auto result_type{ir_builder.getIntNTy(selected_memory_is_address64() ? 64u : 32u)};
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
            return ir_builder.CreateZExtOrTrunc(page_count, result_type);
        }};

    // Emit memory.size for a directly addressable memory.
    auto const emit_direct_memory_page_count_value{[&]() constexpr noexcept -> ::llvm::Value*
                                                   {
                                                       auto memory_length_load{emit_direct_memory_byte_length_value()};
                                                       if(memory_length_load == nullptr) [[unlikely]] { return nullptr; }
                                                       if constexpr(runtime_native_memory_t::can_mmap)
                                                       {
                                                           // This is memory.size / grow(0), not an access-bounds snapshot.
                                                           // Resolve sharedness during compilation: no guest-time flag load.
                                                           if([]<typename Memory>(Memory const& memory) noexcept
                                                           {
                                                               if constexpr(requires { memory.sequentially_consistent_size; })
                                                               { return memory.sequentially_consistent_size; }
                                                               else { return false; }
                                                           }(*selected_memory_access_info.memory_p))
                                                           { ::llvm::cast<::llvm::LoadInst>(memory_length_load)->setAtomic(::llvm::AtomicOrdering::SequentiallyConsistent); }
                                                       }
                                                       return emit_direct_memory_page_count_from_byte_length(memory_length_load,
                                                                                                             static_cast<::std::size_t>(1uz)
                                                                                                                 << selected_memory_access_info.custom_page_size_log2);
                                                   }};


    // Ask a local-imported memory provider for a snapshot and return it as LLVM values.  This is safe for size queries but
    // not used for actual loads/stores because the provider's access lock/snapshot lifetime is not represented in LLVM IR.
    auto const emit_local_imported_memory_snapshot{
        [&]() constexpr noexcept -> llvm_jit_memory_snapshot_values_t
        {
            llvm_jit_memory_snapshot_values_t result{};
            if(!ensure_selected_memory_access_info() || selected_memory_access_info.local_imported_module_ptr == nullptr) [[unlikely]] { return result; }

            static_assert(sizeof(::std::size_t) == sizeof(::std::uintptr_t),
                          "local-imported snapshot ABI represents size_t operands with LLVM intptr");
            static_assert(::std::numeric_limits<::std::size_t>::digits == ::std::numeric_limits<::std::uintptr_t>::digits,
                          "local-imported snapshot ABI requires size_t and uintptr_t to have the same value width");
            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            // The provider writes snapshot outputs through host bridge pointer arguments.  Entry-block allocas give LLVM
            // stable addresses for those out-parameters and make the following loads explicit in IR.
            auto byte_length_slot{
                create_llvm_jit_entry_block_alloca(ir_builder, llvm_intptr_type, nullptr, get_llvm_string_ref(u8"local_imported.memory.byte_length.slot"))};
            if(byte_length_slot == nullptr) [[unlikely]] { return result; }
            auto bridge_function_type{::llvm::FunctionType::get(
                llvm_intptr_type,
                {llvm_intptr_type, llvm_intptr_type, get_llvm_pointer_type(llvm_intptr_type)},
                false)};
            auto module_address{emit_local_imported_memory_module_address()};
            if(module_address == nullptr) [[unlikely]] { return result; }
            auto snapshot_status{emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_snapshot_bridge>(
                bridge_function_type,
                {module_address,
                 ::llvm::ConstantInt::get(llvm_intptr_type, selected_memory_access_info.local_imported_memory_index),
                 byte_length_slot})};
            if(snapshot_status == nullptr) [[unlikely]] { return result; }

            emit_llvm_conditional_trap(*llvm_module,
                                       ir_builder,
                                       ir_builder.CreateICmpEQ(snapshot_status, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)),
                                       ::uwvm2::runtime::lib::llvm_jit_trap_kind::memory_out_of_bounds);
            result.byte_length = ir_builder.CreateLoad(llvm_intptr_type, byte_length_slot, get_llvm_string_ref(u8"local_imported.memory.byte_length"));
            return result;
        }};

    // Emit memory.size for local-imported memories by snapshotting byte length and converting it to pages.
    auto const emit_local_imported_memory_page_count_value{
        [&]() constexpr noexcept -> ::llvm::Value*
        {
            if(!ensure_selected_memory_access_info() || selected_memory_access_info.local_imported_module_ptr == nullptr) [[unlikely]] { return nullptr; }
            if(selected_memory_access_info.local_imported_page_size_bytes == 0u) [[unlikely]] { return nullptr; }

            auto const snapshot{emit_local_imported_memory_snapshot()};
            if(snapshot.byte_length == nullptr) [[unlikely]] { return nullptr; }

            return emit_direct_memory_page_count_from_byte_length(snapshot.byte_length,
                                                                  static_cast<::std::size_t>(selected_memory_access_info.local_imported_page_size_bytes));
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
            if(selected_memory_access_info.local_imported_module_ptr == nullptr || llvm_result_type == nullptr || address_value == nullptr) [[unlikely]]
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
            // ArrayRef is a borrowed view; a named array must own these operands through the complete switch/call.
            ::llvm::Value* bridge_arguments_array[]{
                module_address,
                ::llvm::ConstantInt::get(llvm_intptr_type, selected_memory_access_info.local_imported_memory_index),
                ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), static_offset),
                address_value,
            };
            auto const bridge_arguments{::llvm::ArrayRef<::llvm::Value*>{bridge_arguments_array}};

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
            if(selected_memory_access_info.local_imported_module_ptr == nullptr || llvm_value_type == nullptr || address_value == nullptr || value == nullptr)
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
            // ArrayRef is a borrowed view; a named array must own these operands through the complete switch/call.
            ::llvm::Value* bridge_arguments_array[]{
                module_address,
                ::llvm::ConstantInt::get(llvm_intptr_type, selected_memory_access_info.local_imported_memory_index),
                ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), static_offset),
                address_value,
                value,
            };
            auto const bridge_arguments{::llvm::ArrayRef<::llvm::Value*>{bridge_arguments_array}};

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

    // Native scalar/SIMD accesses share one protection proof. Providers and moving shared allocations retain
    // their access-lifetime bridge; stable mmap and single-thread native memory are directly addressable.
    auto const emit_direct_memory_byte_pointer{
        [&](::std::uint64_t offset, ::std::size_t size, ::llvm::Value* address, bool is_store = false) noexcept -> ::llvm::Value*
        { return emit_llvm_jit_direct_memory_pointer(state, offset, size, address, is_store); }};

    // Emit a native-memory load bridge call for fallback paths.
    auto const emit_native_memory_load_bridge_call{
        [&]<auto bridge_function>(validation_module_traits_t::wasm_u32 static_offset,
                                  ::llvm::Type* llvm_result_type,
                                  ::llvm::Value* address_value,
                                  ::uwvm2::utils::container::u8string_view discriminator) constexpr noexcept -> ::llvm::CallInst*
        {
            if(selected_memory_access_info.memory_p == nullptr || llvm_result_type == nullptr || address_value == nullptr) [[unlikely]] { return nullptr; }

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
            if(selected_memory_access_info.memory_p == nullptr || llvm_value_type == nullptr || address_value == nullptr || value == nullptr) [[unlikely]]
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
            if(selected_memory_access_info.memory_p == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{::llvm::FunctionType::get(ir_builder.getIntNTy(selected_memory_is_address64() ? 64u : 32u), {llvm_intptr_type}, false)};
            auto memory_address{emit_native_memory_object_address()};
            if(memory_address == nullptr) [[unlikely]] { return nullptr; }
            if(selected_memory_is_address64())
            { return emit_runtime_bridge_call.template operator()<llvm_jit_memory64_size_bridge>(bridge_function_type, {memory_address}); }
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
            if(selected_memory_access_info.memory_p != nullptr)
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
            if(selected_memory_access_info.memory_p != nullptr)
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

    // Scalar loads/stores use exact-width integers and a target-endian conversion, including floating bit patterns.
    // Big-endian mmap targets need byte swaps, not a host bridge per instruction.
    auto const emit_direct_memory_load_value{
        [&](::llvm::Value* pointer, runtime_operand_stack_value_type result_type, ::std::size_t load_bytes,
            bool signed_load, ::llvm::Align alignment) noexcept -> ::llvm::Value*
        {
            if(pointer == nullptr || (load_bytes != 1uz && load_bytes != 2uz && load_bytes != 4uz && load_bytes != 8uz)) { return nullptr; }
            auto type{get_llvm_type_from_wasm_value_type(llvm_context, result_type)};
            if(type == nullptr || load_bytes * 8uz > type->getScalarSizeInBits()) { return nullptr; }
            auto integer_type{ir_builder.getIntNTy(static_cast<unsigned>(load_bytes * 8uz))};
            auto load{ir_builder.CreateLoad(integer_type, ir_builder.CreatePointerCast(pointer, get_llvm_pointer_type(integer_type)), "memory.load")};
            load->setAlignment(alignment);
            load->setVolatile(true);
            auto value{simd_ir::emitter{ir_builder}.endian(load)};
            if(type->isFloatingPointTy())
            {
                if(load_bytes * 8uz != type->getScalarSizeInBits()) { return nullptr; }
                return ir_builder.CreateBitCast(value, type);
            }
            return ir_builder.CreateIntCast(value, type, signed_load);
        }};
    auto const emit_direct_memory_store_value{
        [&](::llvm::Value* pointer, runtime_operand_stack_value_type value_type, ::llvm::Value* value,
            ::std::size_t store_bytes, ::llvm::Align alignment) noexcept -> ::llvm::StoreInst*
        {
            static_cast<void>(value_type);
            if(pointer == nullptr || value == nullptr ||
               (store_bytes != 1uz && store_bytes != 2uz && store_bytes != 4uz && store_bytes != 8uz)) { return nullptr; }
            auto bits{value->getType()->getScalarSizeInBits()};
            if(store_bytes * 8uz > bits) { return nullptr; }
            auto integer_type{ir_builder.getIntNTy(static_cast<unsigned>(store_bytes * 8uz))};
            auto value_bits{ir_builder.CreateBitCast(value, ir_builder.getIntNTy(bits))};
            auto stored{simd_ir::emitter{ir_builder}.endian(ir_builder.CreateIntCast(value_bits, integer_type, false))};
            return finalize_llvm_jit_direct_memory_store(
                ir_builder.CreateStore(stored, ir_builder.CreatePointerCast(pointer, get_llvm_pointer_type(integer_type))), alignment);
        }};

    // Emit memory.size for whichever default-memory representation was resolved.
    auto const emit_memory_page_count_value{[&]() constexpr noexcept -> ::llvm::Value*
                                            {
                                                if(!ensure_selected_memory_access_info()) [[unlikely]] { return nullptr; }
                                                if(selected_memory_access_info.local_imported_module_ptr != nullptr)
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

            auto llvm_pages_type{::llvm::cast<::llvm::IntegerType>(delta_value->getType())};
            auto delta_is_zero{ir_builder.CreateICmpEQ(delta_value, ::llvm::ConstantInt::get(llvm_pages_type, 0u))};
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
            auto grow_result_phi{ir_builder.CreatePHI(llvm_pages_type,
                                                      3u,
                                                      local_imported_path ? get_llvm_string_ref(u8"memory.grow.local_imported.result")
                                                                          : get_llvm_string_ref(u8"memory.grow.result"))};
            grow_result_phi->addIncoming(current_page_count, grow_zero_block);
            grow_result_phi->addIncoming(::llvm::ConstantInt::getSigned(llvm_pages_type, -1), grow_fail_block);
            grow_result_phi->addIncoming(bridge_call, grow_runtime_block);
            return grow_result_phi;
        }};

    // Return the effective page size for the resolved default memory.
    auto const get_memory_page_size_bytes{[&]() constexpr noexcept -> ::std::size_t
                                          {
                                              if(selected_memory_access_info.local_imported_module_ptr != nullptr)
                                              {
                                                  return static_cast<::std::size_t>(selected_memory_access_info.local_imported_page_size_bytes);
                                              }
                                              return static_cast<::std::size_t>(1uz) << selected_memory_access_info.custom_page_size_log2;
                                          }};

    // Emit a conservative pre-check for memory.grow requests that cannot fit under the configured byte limit.
    auto const emit_memory_grow_definitely_fail_value{
        [&](::llvm::Value* current_page_count, ::llvm::Value* delta_pages_unsigned) constexpr noexcept -> ::llvm::Value*
        {
            if(current_page_count == nullptr || delta_pages_unsigned == nullptr) [[unlikely]] { return nullptr; }

            auto const page_size_bytes{get_memory_page_size_bytes()};
            if(selected_memory_access_info.max_limit_memory_length == ::std::numeric_limits<::std::size_t>::max() || page_size_bytes == 0uz)
            {
                return ::llvm::ConstantInt::getFalse(llvm_context);
            }

            // Use the Wasm delta width: a 32-bit host must still reject high i64 deltas.
            auto llvm_intptr_type{::llvm::cast<::llvm::IntegerType>(delta_pages_unsigned->getType())};
            auto current_page_count_unsigned{ir_builder.CreateZExtOrTrunc(current_page_count, llvm_intptr_type)};
            auto const limit_pages{selected_memory_access_info.max_limit_memory_length / page_size_bytes};
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

            if(selected_memory_access_info.local_imported_module_ptr != nullptr)
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
                     ::llvm::ConstantInt::get(llvm_intptr_type, selected_memory_access_info.local_imported_memory_index),
                     ::llvm::ConstantInt::get(llvm_intptr_type, selected_memory_access_info.max_limit_memory_length),
                     delta_value});
            }

            if(selected_memory_access_info.memory_p == nullptr) [[unlikely]] { return nullptr; }
            if(selected_memory_is_address64())
            {
                auto const type{::llvm::FunctionType::get(ir_builder.getInt64Ty(),
                    {llvm_intptr_type, llvm_intptr_type, ir_builder.getInt64Ty()}, false)};
                auto const owner{emit_native_memory_object_address()};
                if(owner == nullptr) { return nullptr; }
                return emit_runtime_bridge_call.template operator()<llvm_jit_memory64_grow_bridge>(type,
                    {owner, ::llvm::ConstantInt::get(llvm_intptr_type, selected_memory_access_info.max_limit_memory_length), delta_value});
            }

            auto bridge_function_type{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(llvm_context),
                                                                {llvm_intptr_type, llvm_intptr_type, ::llvm::Type::getInt32Ty(llvm_context)},
                                                                false)};
            auto memory_address{emit_native_memory_object_address()};
            if(memory_address == nullptr) [[unlikely]] { return nullptr; }
            return emit_runtime_bridge_call.template operator()<llvm_jit_memory_grow_bridge>(
                bridge_function_type,
                {memory_address, ::llvm::ConstantInt::get(llvm_intptr_type, selected_memory_access_info.max_limit_memory_length), delta_value});
        }};

    // Complete a Wasm memory load instruction after the opcode case provides static offset, alignment, result type, width,
    // signedness, and the native bridge function.
    auto const emit_memory_load_call{
        [&]<auto bridge_function, unsigned ValueBits, ::std::size_t Bytes, bool Signed = false>(::std::uint64_t static_offset,
                                  validation_module_traits_t::wasm_u32 memarg_align,
                                  runtime_operand_stack_value_type result_type,
                                  ::llvm::Type* llvm_result_type,
                                  ::std::size_t load_bytes,
                                  bool signed_load) constexpr noexcept -> bool
        {
            if(!ensure_selected_memory_access_info() || llvm_result_type == nullptr || operand_stack.empty()) [[unlikely]] { return false; }

            auto const address{operand_stack.back()};
            operand_stack.pop_back();
            if(address.type != (selected_memory_is_address64() ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32) || address.value == nullptr) [[unlikely]] { return false; }

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

            if(selected_memory_is_address64())
            {
                auto const owner{emit_native_memory_object_address()};
                auto const loaded{emit_llvm_jit_memory64_scalar_call<ValueBits, Bytes, Signed, false>(ir_builder,
                    owner, ir_builder.getInt64(static_offset), address.value, llvm_result_type)};
                if(loaded == nullptr) { return false; }
                push_operand(result_type, loaded);
                return true;
            }
            // The memory32 replayed type bounds the offset before this native ABI narrowing.
            auto bridge_call{emit_memory_load_bridge_fallback_call
                                 .template operator()<bridge_function>(static_cast<validation_module_traits_t::wasm_u32>(static_offset), result_type, llvm_result_type, load_bytes, signed_load, address.value)};
            if(bridge_call == nullptr) [[unlikely]] { return false; }

            push_operand(result_type, bridge_call);
            return true;
        }};

    // Complete a Wasm memory store instruction after the opcode case provides static offset, alignment, value type, width,
    // and the native bridge function.
    auto const emit_memory_store_call{
        [&]<auto bridge_function, unsigned ValueBits, ::std::size_t Bytes, bool Signed = false>(::std::uint64_t static_offset,
                                  validation_module_traits_t::wasm_u32 memarg_align,
                                  runtime_operand_stack_value_type value_type,
                                  ::llvm::Type* llvm_value_type,
                                  ::std::size_t store_bytes) constexpr noexcept -> bool
        {
            if(!ensure_selected_memory_access_info() || llvm_value_type == nullptr || operand_stack.size() < 2uz) [[unlikely]] { return false; }

            auto const value{operand_stack.back()};
            operand_stack.pop_back();
            auto const address{operand_stack.back()};
            operand_stack.pop_back();

            if(value.type != value_type || address.type != (selected_memory_is_address64() ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32) || value.value == nullptr || address.value == nullptr)
                [[unlikely]]
            {
                return false;
            }

            {
                auto direct_memory_pointer{emit_direct_memory_byte_pointer(static_offset, store_bytes, address.value, true)};

                if(direct_memory_pointer != nullptr)
                {
                    auto const memory_alignment{get_llvm_wasm_memory_access_alignment(store_bytes, memarg_align)};
                    return emit_direct_memory_store_value(direct_memory_pointer, value_type, value.value, store_bytes, memory_alignment) != nullptr;
                }
            }

            if(selected_memory_is_address64())
            {
                auto const owner{emit_native_memory_object_address()};
                return emit_llvm_jit_memory64_scalar_call<ValueBits, Bytes, false, true>(ir_builder,
                    owner, ir_builder.getInt64(static_offset), address.value, llvm_value_type, value.value) != nullptr;
            }
            // The memory32 replayed type bounds the offset before this native ABI narrowing.
            return emit_memory_store_bridge_fallback_call
                       .template operator()<bridge_function>(static_cast<validation_module_traits_t::wasm_u32>(static_offset), value_type, llvm_value_type, store_bytes, address.value, value.value) != nullptr;
        }};

    auto const emit_native_memory_copy_bridge_call{
        [&](::llvm::Value* dst, ::llvm::Value* src, ::llvm::Value* len) constexpr noexcept -> ::llvm::CallInst*
        {
            if(!ensure_selected_memory_access_info() || selected_memory_access_info.memory_p == nullptr || dst == nullptr || src == nullptr || len == nullptr) [[unlikely]]
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
            if(!ensure_selected_memory_access_info() || selected_memory_access_info.memory_p == nullptr || dst == nullptr || value == nullptr || len == nullptr) [[unlikely]]
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
            if(!ensure_selected_memory_access_info() || selected_memory_access_info.local_imported_module_ptr == nullptr || dst == nullptr || src == nullptr || len == nullptr)
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
                 ::llvm::ConstantInt::get(llvm_intptr_type, selected_memory_access_info.local_imported_memory_index),
                 dst,
                 src,
                 len});
        }};

    auto const emit_local_imported_memory_fill_bridge_call{
        [&](::llvm::Value* dst, ::llvm::Value* value, ::llvm::Value* len) constexpr noexcept -> ::llvm::CallInst*
        {
            if(!ensure_selected_memory_access_info() || selected_memory_access_info.local_imported_module_ptr == nullptr || dst == nullptr || value == nullptr || len == nullptr)
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
                 ::llvm::ConstantInt::get(llvm_intptr_type, selected_memory_access_info.local_imported_memory_index),
                 dst,
                 value,
                 len});
        }};

    auto const emit_memory_copy_call{[&](validation_module_traits_t::wasm_u32 source_memory_index) constexpr noexcept -> bool
                                     {
                                         if(operand_stack.size() < 3uz) [[unlikely]] { return false; }

                                         auto const len{operand_stack.back()};
                                         operand_stack.pop_back();
                                         auto const src{operand_stack.back()};
                                         operand_stack.pop_back();
                                         auto const dst{operand_stack.back()};
                                         operand_stack.pop_back();

                                         auto const module{local_func_storage.runtime_module_ptr};
                                         if(module == nullptr) { return false; }
                                         bool const destination64{selected_memory_is_address64()};
                                         bool const source64{::uwvm2::uwvm::runtime::storage::runtime_memory_is_address64(*module, source_memory_index)};
                                         auto const destination_type{destination64 ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32};
                                         auto const source_type{source64 ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32};
                                         auto const length_type{destination64 && source64 ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32};
                                         if(dst.type != destination_type || src.type != source_type || len.type != length_type ||
                                            dst.value == nullptr || src.value == nullptr || len.value == nullptr) [[unlikely]]
                                         {
                                             return false;
                                         }

                                         if(!ensure_selected_memory_access_info()) [[unlikely]] { return false; }

                                         if(destination64 || source64)
                                         {
                                             auto const source_info{resolve_runtime_memory_access_info(*module, source_memory_index)};
                                             auto const native{ir_builder.getIntNTy(sizeof(::std::uintptr_t) * CHAR_BIT)};
                                             auto const wide{ir_builder.getInt64Ty()};
                                             auto const destination{ir_builder.CreateZExtOrTrunc(dst.value, wide)};
                                             auto const source{ir_builder.CreateZExtOrTrunc(src.value, wide)};
                                             auto const length{ir_builder.CreateZExtOrTrunc(len.value, wide)};
                                             if(selected_memory_access_info.memory_p != nullptr && source_info.memory_p != nullptr)
                                             {
                                                 auto const dst_owner{emit_native_memory_object_address()};
                                                 auto const name{get_llvm_native_memory_object_symbol_name(*module, source_memory_index)};
                                                 auto const src_owner{get_llvm_external_host_object_address(ir_builder,
                                                     reinterpret_cast<::std::uintptr_t>(source_info.memory_p),
                                                     ::uwvm2::utils::container::u8string_view{name.data(), name.size()})};
                                                 if(dst_owner == nullptr || src_owner == nullptr) { return false; }
                                                 auto const signature{::llvm::FunctionType::get(ir_builder.getVoidTy(), {native, native, wide, wide, wide}, false)};
                                                 auto const bridge_call{emit_runtime_bridge_call.template operator()<llvm_jit_memory64_copy_bridge<true, true>>(
                                                     signature, {dst_owner, src_owner, destination, source, length})};
                                                 return bridge_call != nullptr;
                                             }
                                             auto const module_address{emit_runtime_module_object_address()};
                                             if(module_address == nullptr) { return false; }
                                             auto const signature{::llvm::FunctionType::get(ir_builder.getVoidTy(), {native, native, native, wide, wide, wide}, false)};
                                             auto const bridge_call{emit_runtime_bridge_call.template operator()<llvm_jit_wide_cross_provider_memory_copy_bridge>(signature,
                                                 {module_address, ::llvm::ConstantInt::get(native, state.current_memory_index),
                                                  ::llvm::ConstantInt::get(native, source_memory_index), destination, source, length})};
                                             return bridge_call != nullptr;
                                         }

                                         if(source_memory_index != state.current_memory_index)
                                         {
                                             auto runtime_module{local_func_storage.runtime_module_ptr};
                                             if(runtime_module == nullptr) { return false; }
                                             auto const source_info{resolve_runtime_memory_access_info(*runtime_module, source_memory_index)};
                                             auto const llvm_void{::llvm::Type::getVoidTy(llvm_context)};
                                             auto const llvm_i32{::llvm::Type::getInt32Ty(llvm_context)};
                                             auto const llvm_intptr{::llvm::Type::getIntNTy(llvm_context, sizeof(::std::uintptr_t) * 8u)};
                                             if(selected_memory_access_info.memory_p != nullptr && source_info.memory_p != nullptr)
                                             {
                                                 auto const destination_address{emit_native_memory_object_address()};
                                                 auto const source_symbol{get_llvm_native_memory_object_symbol_name(*runtime_module, source_memory_index)};
                                                 auto const source_address{get_llvm_external_host_object_address(ir_builder,
                                                     reinterpret_cast<::std::uintptr_t>(source_info.memory_p),
                                                     ::uwvm2::utils::container::u8string_view{source_symbol.data(), source_symbol.size()})};
                                                 if(destination_address == nullptr || source_address == nullptr) { return false; }
                                                 auto const signature{::llvm::FunctionType::get(llvm_void, {llvm_intptr, llvm_intptr, llvm_i32, llvm_i32, llvm_i32}, false)};
                                                 ::llvm::Value* arguments[]{destination_address, source_address, dst.value, src.value, len.value};
                                                 return emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_cross_native_memory_copy_bridge>(state, signature, {arguments}) != nullptr;
                                             }
                                             auto const module_address{emit_runtime_module_object_address()};
                                             if(module_address == nullptr) { return false; }
                                             auto const signature{::llvm::FunctionType::get(llvm_void, {llvm_intptr, llvm_i32, llvm_i32, llvm_i32, llvm_i32, llvm_i32}, false)};
                                             ::llvm::Value* arguments[]{module_address, ::llvm::ConstantInt::get(llvm_i32, state.current_memory_index),
                                                  ::llvm::ConstantInt::get(llvm_i32, source_memory_index), dst.value, src.value, len.value};
                                             return emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_cross_provider_memory_copy_bridge>(state, signature, {arguments}) != nullptr;
                                         }
                                         if(selected_memory_access_info.memory_p != nullptr)
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

                                         auto const address_type{selected_memory_is_address64() ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32};
                                         if(dst.type != address_type || value.type != runtime_operand_stack_value_type::i32 ||
                                            len.type != address_type || dst.value == nullptr || value.value == nullptr || len.value == nullptr) [[unlikely]]
                                         {
                                             return false;
                                         }

                                         if(!ensure_selected_memory_access_info()) [[unlikely]] { return false; }
                                         if(selected_memory_is_address64())
                                         {
                                             if(selected_memory_access_info.memory_p != nullptr)
                                             {
                                                 auto const owner{emit_native_memory_object_address()};
                                                 if(owner == nullptr) { return false; }
                                                 auto const native{ir_builder.getIntNTy(sizeof(::std::uintptr_t) * CHAR_BIT)};
                                                 auto const signature{::llvm::FunctionType::get(ir_builder.getVoidTy(),
                                                     {native, ir_builder.getInt64Ty(), ir_builder.getInt32Ty(), ir_builder.getInt64Ty()}, false)};
                                                 auto const bridge_call{emit_runtime_bridge_call.template operator()<llvm_jit_memory64_fill_bridge<true>>(
                                                     signature, {owner, dst.value, value.value, len.value})};
                                                 return bridge_call != nullptr;
                                             }
                                             auto const provider{emit_local_imported_memory_module_address()};
                                             if(provider == nullptr) { return false; }
                                             auto const native{ir_builder.getIntNTy(sizeof(::std::uintptr_t) * CHAR_BIT)};
                                             auto const signature{::llvm::FunctionType::get(ir_builder.getVoidTy(),
                                                 {native, native, ir_builder.getInt64Ty(), ir_builder.getInt32Ty(), ir_builder.getInt64Ty()}, false)};
                                             auto const bridge_call{emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory64_fill_bridge>(
                                                 signature, {provider, ::llvm::ConstantInt::get(native, selected_memory_access_info.local_imported_memory_index),
                                                     dst.value, value.value, len.value})};
                                             return bridge_call != nullptr;
                                         }
                                         if(selected_memory_access_info.memory_p != nullptr)
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
                                         if(!ensure_selected_memory_access_info() || operand_stack.size() < 3uz) [[unlikely]]
                                         {
                                             return false;
                                         }

                                         auto const len{operand_stack.back()};
                                         operand_stack.pop_back();
                                         auto const src{operand_stack.back()};
                                         operand_stack.pop_back();
                                         auto const dst{operand_stack.back()};
                                         operand_stack.pop_back();
                                         if(dst.type != (selected_memory_is_address64() ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32) || src.type != runtime_operand_stack_value_type::i32 ||
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

                                         if(selected_memory_is_address64())
                                         {
                                             if(selected_memory_access_info.memory_p != nullptr)
                                             {
                                                 auto const owner{emit_native_memory_object_address()};
                                                 if(owner == nullptr) { return false; }
                                                 auto const carrier{ir_builder.getIntNTy(sizeof(llvm_jit_atomic_address_carrier_t) * CHAR_BIT)};
                                                 auto const signature{::llvm::FunctionType::get(llvm_void_type,
                                                     {llvm_intptr_type, llvm_intptr_type, ir_builder.getInt64Ty(), ir_builder.getInt64Ty(), carrier, carrier}, false)};
                                                 auto const bridge_call{emit_runtime_bridge_call.template operator()<llvm_jit_memory64_module_init_bridge>(signature,
                                                     {owner, module_address, ir_builder.getInt64(data_index), dst.value,
                                                      ir_builder.CreateZExtOrTrunc(src.value, carrier), ir_builder.CreateZExtOrTrunc(len.value, carrier)})};
                                                 return bridge_call != nullptr;
                                             }
                                             auto const provider{emit_local_imported_memory_module_address()};
                                             if(provider == nullptr) { return false; }
                                             auto const carrier{ir_builder.getIntNTy(sizeof(llvm_jit_atomic_address_carrier_t) * CHAR_BIT)};
                                             auto const signature{::llvm::FunctionType::get(llvm_void_type,
                                                 {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, ir_builder.getInt64Ty(), ir_builder.getInt64Ty(), carrier, carrier}, false)};
                                             auto const bridge_call{emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory64_init_bridge>(signature,
                                                 {provider, ::llvm::ConstantInt::get(llvm_intptr_type, selected_memory_access_info.local_imported_memory_index),
                                                     module_address, ir_builder.getInt64(data_index), dst.value,
                                                     ir_builder.CreateZExtOrTrunc(src.value, carrier), ir_builder.CreateZExtOrTrunc(len.value, carrier)})};
                                             return bridge_call != nullptr;
                                         }

                                         if(selected_memory_access_info.local_imported_module_ptr != nullptr)
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
                                                 ::llvm::ConstantInt::get(llvm_intptr_type, selected_memory_access_info.local_imported_memory_index),
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
                                        if(dst.type != table_operand_type(table_index) || src.type != runtime_operand_stack_value_type::i32 ||
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
                                             llvm_i32_type, dst.value->getType(), src.value->getType(), len.value->getType()},
                                            false)};
                                        ::llvm::Value* bridge_arguments[]{
                                            module_address,
                                            ::llvm::ConstantInt::get(llvm_i32_type, element_index),
                                            ::llvm::ConstantInt::get(llvm_i32_type, table_index),
                                            dst.value,
                                            src.value,
                                            len.value};
                                        auto const bridge_call{emit_table_width_bridge_call.template operator()<&llvm_jit_table_init_bridge<runtime_wasm_i32>, &llvm_jit_table_init_bridge<runtime_wasm_i64>>(table_address64_at(table_index), bridge_function_type, {bridge_arguments})};
                                        return bridge_call != nullptr;
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
                                        if(dst.type != table_operand_type(dst_table_index) || src.type != table_operand_type(src_table_index) ||
                                           len.type != (table_address64_at(dst_table_index) && table_address64_at(src_table_index) ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32) || dst.value == nullptr || src.value == nullptr ||
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
                                             llvm_i32_type, dst.value->getType(), src.value->getType(), len.value->getType()},
                                            false)};
                                        ::llvm::Value* bridge_arguments[]{
                                            module_address,
                                            ::llvm::ConstantInt::get(llvm_i32_type, dst_table_index),
                                            ::llvm::ConstantInt::get(llvm_i32_type, src_table_index),
                                            dst.value,
                                            src.value,
                                            len.value};
                                        return (table_address64_at(dst_table_index) ?
                                            emit_table_width_bridge_call.template operator()<&llvm_jit_table_copy_bridge<runtime_wasm_i64, runtime_wasm_i32>, &llvm_jit_table_copy_bridge<runtime_wasm_i64, runtime_wasm_i64>>(
                                                table_address64_at(src_table_index), bridge_function_type, {bridge_arguments}) :
                                            emit_table_width_bridge_call.template operator()<&llvm_jit_table_copy_bridge<runtime_wasm_i32, runtime_wasm_i32>, &llvm_jit_table_copy_bridge<runtime_wasm_i32, runtime_wasm_i64>>(
                                                table_address64_at(src_table_index), bridge_function_type, {bridge_arguments})) != nullptr;
                                    }};

    auto const emit_table_grow_call{[&](validation_module_traits_t::wasm_u32 table_index,
                                        runtime_operand_stack_value_type reference_value_type) constexpr noexcept -> bool
                                    {
                                        if(operand_stack.size() < 2uz) [[unlikely]] { return false; }
                                        auto const delta{operand_stack.back()};
                                        operand_stack.pop_back();
                                        auto const value{operand_stack.back()};
                                        operand_stack.pop_back();
                                        if(delta.type != table_operand_type(table_index) || delta.value == nullptr ||
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
                                            delta.value->getType(),
                                            {llvm_intptr_type, llvm_i32_type, llvm_intptr_type, delta.value->getType()},
                                            false)};
                                        ::llvm::Value* bridge_arguments[]{
                                            module_address,
                                            ::llvm::ConstantInt::get(llvm_i32_type, table_index),
                                            value_address,
                                            delta.value};
                                        auto result{emit_table_width_bridge_call.template operator()<&llvm_jit_table_grow_bridge<runtime_wasm_i32>, &llvm_jit_table_grow_bridge<runtime_wasm_i64>>(table_address64_at(table_index), bridge_function_type, {bridge_arguments})};
                                        if(result == nullptr) [[unlikely]] { return false; }
                                        push_operand(table_operand_type(table_index), result);
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
                                            ::llvm::FunctionType::get(get_llvm_type_from_wasm_value_type(llvm_context, table_operand_type(table_index)), {llvm_intptr_type, llvm_i32_type}, false)};
                                        ::llvm::Value* bridge_arguments[]{
                                            module_address,
                                            ::llvm::ConstantInt::get(llvm_i32_type, table_index)};
                                        auto result{emit_table_width_bridge_call.template operator()<&llvm_jit_table_size_bridge<runtime_wasm_i32>, &llvm_jit_table_size_bridge<runtime_wasm_i64>>(table_address64_at(table_index), bridge_function_type, {bridge_arguments})};
                                        if(result == nullptr) [[unlikely]] { return false; }
                                        push_operand(table_operand_type(table_index), result);
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
                                        if(dst.type != table_operand_type(table_index) || dst.value == nullptr ||
                                           value.type != reference_value_type || value.value == nullptr ||
                                           len.type != table_operand_type(table_index) || len.value == nullptr) [[unlikely]]
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
                                            {llvm_intptr_type, llvm_i32_type, dst.value->getType(), llvm_intptr_type, len.value->getType()},
                                            false)};
                                        ::llvm::Value* bridge_arguments[]{
                                            module_address,
                                            ::llvm::ConstantInt::get(llvm_i32_type, table_index),
                                            dst.value,
                                            value_address,
                                            len.value};
                                        auto const bridge_call{emit_table_width_bridge_call.template operator()<&llvm_jit_table_fill_bridge<runtime_wasm_i32>, &llvm_jit_table_fill_bridge<runtime_wasm_i64>>(table_address64_at(table_index), bridge_function_type, {bridge_arguments})};
                                        return bridge_call != nullptr;
                                    }};

    // Complete a Wasm memory.size instruction.
    auto const emit_memory_size_call{[&]() constexpr noexcept -> bool
                                     {
                                         auto page_count{emit_memory_page_count_value()};
                                         if(page_count == nullptr) [[unlikely]] { return false; }

                                         push_operand(selected_memory_is_address64() ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32, page_count);
                                         return true;
                                     }};

    // Complete a Wasm memory.grow instruction.
    auto const emit_memory_grow_call{[&]() constexpr noexcept -> bool
                                     {
                                         if(!ensure_selected_memory_access_info() || operand_stack.empty()) [[unlikely]] { return false; }

                                         auto const delta{operand_stack.back()};
                                         operand_stack.pop_back();
                                         if(delta.type != (selected_memory_is_address64() ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32) || delta.value == nullptr) [[unlikely]] { return false; }

                                         auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
                                         auto current_page_count{emit_memory_page_count_value()};
                                         if(current_page_count == nullptr) [[unlikely]] { return false; }
                                         auto delta_pages_unsigned{selected_memory_is_address64() ? delta.value : ir_builder.CreateZExtOrTrunc(delta.value, llvm_intptr_type)};
                                         auto definitely_fail{emit_memory_grow_definitely_fail_value(current_page_count, delta_pages_unsigned)};
                                         if(definitely_fail == nullptr) [[unlikely]] { return false; }

                                         auto grow_result_phi{emit_memory_grow_result_value(delta.value,
                                                                                            current_page_count,
                                                                                            definitely_fail,
                                                                                            selected_memory_access_info.local_imported_module_ptr != nullptr,
                                                                                            [&]() constexpr noexcept -> ::llvm::CallInst*
                                                                                            { return emit_memory_grow_bridge_call(delta.value); })};
                                         if(grow_result_phi == nullptr) [[unlikely]] { return false; }

                                         push_operand(selected_memory_is_address64() ? runtime_operand_stack_value_type::i64 : runtime_operand_stack_value_type::i32, grow_result_phi);
                                         return true;
                                     }};

    auto const emit_checked_atomic_body{[&](::uwvm2::validation::standard::wasm3::atomic_instruction64_immediate const& instruction) constexpr noexcept -> bool
    {
        namespace atomic = ::uwvm2::validation::standard::wasm3;
#include "opcode/atomic_typed_emit_body.h"
    }};

    if(control_stack.empty()) [[unlikely]] { return false; }
    if constexpr(NormalizedSimd)
    {
        // Original SIMD opcode ordering, without a raw cursor or immediate scan.
        auto const& event{*simd_event};
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        if(control_stack.back().is_reachable &&
           (!snapshot_runtime_local_func_llvm_jit_gc_roots(state) || !sealed_compact_codegen::retire(state))) { return false; }
#endif
        state.current_wasm_op_offset = event.source_offset;
        if(!emit_runtime_local_func_llvm_jit_native_provenance(state, state.current_wasm_op_offset)) { return false; }
        if(state.pending_numeric_plan != nullptr && !pending_numeric_primary_opcode(0xfdu)) { return false; }
        if(!emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) { return false; }
        if(!control_stack.back().is_reachable) { return true; }
        namespace shared_simd = ::uwvm2::runtime::compiler::shared;
        return shared_simd::visit_wasm1p1_simd_instruction(
            static_cast<wasm1p1_simd_code>(event.opcode),
            [&]<llvm_jit_simd_code Op, shared_simd::wasm1p1_simd_instruction_kind Kind,
                shared_simd::wasm1p1_simd_scalar_kind ScalarKind, ::std::size_t LaneCount,
                ::std::uint_least32_t MaxAlign>() constexpr noexcept -> bool
            {
                if(event.descriptor != llvm_jit_simd_event_descriptor<Kind, ScalarKind, LaneCount, MaxAlign>()) { return false; }
                if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::memory_load ||
                             Kind == shared_simd::wasm1p1_simd_instruction_kind::memory_store)
                { select_memory(event.memory.immediate.memory_index); }
                return emit_runtime_local_func_llvm_jit_typed_simd_instruction<Op, Kind, ScalarKind, LaneCount, MaxAlign>(
                    state, event.memory.immediate.offset, event.lane,
                    event.has_vector ? event.vector_bytes.data() : nullptr);
            });
    }
    else if constexpr(NormalizedScalarMemory)
    {
        // No raw cursor/LEB/scan is executed in this compile-time specialization.
        auto const& event{*memory_event};
        // Preserve the original scalar opcode retirement ordering: every scalar memory
        // operation is outside sealed_pure, and retirement precedes provenance/debug emission.
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        if(control_stack.back().is_reachable &&
           (!snapshot_runtime_local_func_llvm_jit_gc_roots(state) || !sealed_compact_codegen::retire(state))) { return false; }
#endif
        state.current_wasm_op_offset = event.source_offset;
        if(!emit_runtime_local_func_llvm_jit_native_provenance(state, state.current_wasm_op_offset)) { return false; }
        if(state.pending_numeric_plan != nullptr && !pending_numeric_primary_opcode(event.opcode)) { return false; }
        if(!emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) { return false; }
        // The authoritative validator already checked unreachable operands. Ordinary unreachable IR is absent.
        if(!control_stack.back().is_reachable) { return true; }
        select_memory(event.memory_index);
        auto const align{event.alignment};
        auto const offset{event.offset};
        switch(static_cast<wasm1_code>(event.opcode))
        {
#include "opcode/scalar_memory_typed_emit_cases.h"
            default: return false;
        }
    }
    else if constexpr(NormalizedPageMemory)
    {
        // Consume the first typed decoder's selected declaration and opcode;
        // no raw opcode/index/LEB read runs in this specialization.
        auto const& event{*page_event};
        // Preserve the original size/grow retirement BEFORE provenance/debug.
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        if(control_stack.back().is_reachable &&
           (!snapshot_runtime_local_func_llvm_jit_gc_roots(state) || !sealed_compact_codegen::retire(state))) { return false; }
#endif
        state.current_wasm_op_offset = event.source_offset;
        if(!emit_runtime_local_func_llvm_jit_native_provenance(state, state.current_wasm_op_offset)) { return false; }
        if(state.pending_numeric_plan != nullptr && !pending_numeric_primary_opcode(event.opcode)) { return false; }
        if(!emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) { return false; }
        if(!control_stack.back().is_reachable) { return true; }
        select_memory(event.memory_index);
        switch(event.opcode)
        {
            case 0x3fu: return emit_memory_size_call();
            case 0x40u: return emit_memory_grow_call();
            default: return false;
        }
    }
    else if constexpr(NormalizedAtomic)
    {
        // The original FE decoder and common typed stack kernel supplied DATA;
        // there is no opcode/subopcode/memarg replay in this specialization.
        auto const& event{*atomic_event};
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        if(control_stack.back().is_reachable &&
           (!snapshot_runtime_local_func_llvm_jit_gc_roots(state) || !sealed_compact_codegen::retire(state))) { return false; }
#endif
        state.current_wasm_op_offset = event.source_offset;
        if(!emit_runtime_local_func_llvm_jit_native_provenance(state, state.current_wasm_op_offset)) { return false; }
        if(state.pending_numeric_plan != nullptr && !pending_numeric_primary_opcode(0xfeu)) { return false; }
        if(!emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) { return false; }
        if(!control_stack.back().is_reachable) { return true; }
        return emit_checked_atomic_body(event.decoded.immediate);
    }
    else if constexpr(NormalizedBulkMemory)
    {
        // DATA comes from the first bounded FC immediate decoder, checked index
        // metadata, and the common successful typed sequence. No raw replay.
        namespace bulk = ::uwvm2::validation::standard::wasm3;
        auto const& event{*bulk_event};
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        if(control_stack.back().is_reachable &&
           (!snapshot_runtime_local_func_llvm_jit_gc_roots(state) || !sealed_compact_codegen::retire(state))) { return false; }
#endif
        state.current_wasm_op_offset = event.source_offset;
        if(!emit_runtime_local_func_llvm_jit_native_provenance(state, state.current_wasm_op_offset)) { return false; }
        if(state.pending_numeric_plan != nullptr && !pending_numeric_primary_opcode(0xfcu)) { return false; }
        if(!emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) { return false; }
        if(!control_stack.back().is_reachable) { return true; }
        auto const& decoded{event.decoded};
        switch(decoded.kind)
        {
            case bulk::bulk_memory_instruction_kind::init:
                select_memory(decoded.destination_memory_index);
                return emit_memory_init_call(decoded.data_index);
            case bulk::bulk_memory_instruction_kind::data_drop:
                return emit_data_drop_call(decoded.data_index);
            case bulk::bulk_memory_instruction_kind::copy:
                select_memory(decoded.destination_memory_index);
                return emit_memory_copy_call(decoded.source_memory_index);
            case bulk::bulk_memory_instruction_kind::fill:
                select_memory(decoded.destination_memory_index);
                return emit_memory_fill_call();
        }
        return false;
    }
    else if constexpr(NormalizedI32Numeric)
    {
        // The first dispatcher read and shared successful typed transition own
        // this DATA. Check it BEFORE metadata mutation or any body IR.
        // [one same owned typed event] end; no raw Wasm range is accepted.
        // [safe synchronous borrow  ] ^^ numeric_event was non-null checked above.
        auto const& event{*numeric_event};
        if(!llvm_jit_i32_numeric_event_consistent(state, event)) { return false; }
        // i32 0x67..0x78 is already in the original sealed_pure family. Do not
        // retire a compact carrier or insert a new root/guard on this hot path.
        state.current_wasm_op_offset = event.source_offset;
        if(!emit_runtime_local_func_llvm_jit_native_provenance(state, state.current_wasm_op_offset)) { return false; }
        if(state.pending_numeric_plan != nullptr && !pending_numeric_primary_opcode(event.opcode)) { return false; }
        // The original fused dispatcher may have emitted this exact point;
        // existing offset de-duplication keeps its one-call observation order.
        if(!emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) { return false; }
        return emit_runtime_local_func_llvm_jit_i32_numeric_event_body(state, event);
    }
    else if constexpr(NormalizedI64Numeric)
    {
        // The first dispatcher read and shared successful typed transition own
        // this DATA. Check it BEFORE metadata mutation or any body IR.
        // [one same owned typed event] end; no raw Wasm range is accepted.
        // [safe synchronous borrow  ] ^^ numeric64_event was non-null checked above.
        auto const& event{*numeric64_event};
        if(!llvm_jit_i64_numeric_event_consistent(state, event)) { return false; }
        // i64 0x79..0x8a is already in the original sealed_pure family. Do not
        // retire a compact carrier or insert a new root/guard on this hot path.
        state.current_wasm_op_offset = event.source_offset;
        if(!emit_runtime_local_func_llvm_jit_native_provenance(state, state.current_wasm_op_offset)) { return false; }
        if(state.pending_numeric_plan != nullptr && !pending_numeric_primary_opcode(event.opcode)) { return false; }
        // The original fused dispatcher may have emitted this exact point;
        // existing offset de-duplication keeps its one-call observation order.
        if(!emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) { return false; }
        return emit_runtime_local_func_llvm_jit_i64_numeric_event_body(state, event);
    }
    else if constexpr(NormalizedIntegerWidth)
    {
        // The first dispatcher read and shared successful typed transition own
        // this DATA. Check it BEFORE metadata mutation or any body IR.
        // [one same owned typed event] end; no raw Wasm range is accepted.
        // [safe synchronous borrow  ] ^^ width_event was non-null checked above.
        auto const& event{*width_event};
        if(!llvm_jit_integer_width_event_consistent(state, event)) { return false; }
        // integer-width 0xa7/0xac/0xad is already in the original sealed_pure family. Do not
        // retire a compact carrier or insert a new root/guard on this hot path.
        state.current_wasm_op_offset = event.source_offset;
        if(!emit_runtime_local_func_llvm_jit_native_provenance(state, state.current_wasm_op_offset)) { return false; }
        if(state.pending_numeric_plan != nullptr && !pending_numeric_primary_opcode(event.opcode)) { return false; }
        // The original fused dispatcher may have emitted this exact point;
        // existing offset de-duplication keeps its one-call observation order.
        if(!emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) { return false; }
        return emit_runtime_local_func_llvm_jit_integer_width_event_body(state, event);
    }
    else if constexpr(NormalizedIntegerCompare)
    {
        // The first dispatcher read and shared successful typed transition own
        // this DATA. Check it BEFORE metadata mutation or any body IR.
        // [one same owned typed event] end; no raw Wasm range is accepted.
        // [safe synchronous borrow  ] ^^ compare_event was non-null checked above.
        auto const& event{*compare_event};
        if(!llvm_jit_integer_compare_event_consistent(state, event)) { return false; }
        // integer compare 0x45..0x5a is already in the original sealed_pure family. Do not
        // retire a compact carrier or insert a new root/guard on this hot path.
        state.current_wasm_op_offset = event.source_offset;
        if(!emit_runtime_local_func_llvm_jit_native_provenance(state, state.current_wasm_op_offset)) { return false; }
        if(state.pending_numeric_plan != nullptr && !pending_numeric_primary_opcode(event.opcode)) { return false; }
        // The original fused dispatcher may have emitted this exact point;
        // existing offset de-duplication keeps its one-call observation order.
        if(!emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) { return false; }
        return emit_runtime_local_func_llvm_jit_integer_compare_event_body(state, event);
    }
    else if constexpr(NormalizedTableAccess)
    {
        // Actual bounded-first tableidx+shared typed event, never a raw slice.
        // [same synchronous owned DATA] end; above non-null check proves borrow.
        auto const& event{*table_event};
        if(!llvm_jit_table_access_event_consistent(state, event)) { return false; }
        runtime_operand_stack_value_type reference_value_type{};
        if(!get_runtime_table_reference_value_type(event.table_index, reference_value_type) ||
           static_cast<unsigned>(reference_value_type) != event.element_carrier ||
           table_address64_at(event.table_index) != event.address64) { return false; }
        // Original sealed_pure includes BOTH 0x25 and 0x26; preserve that exact
        // compact-carrier policy. Table bridges retain original owner/barriers.
        state.current_wasm_op_offset = event.source_offset;
        if(!emit_runtime_local_func_llvm_jit_native_provenance(state, state.current_wasm_op_offset)) { return false; }
        if(state.pending_numeric_plan != nullptr && !pending_numeric_primary_opcode(event.opcode)) { return false; }
        if(!emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) { return false; }
        // Original raw dispatcher skipped all ordinary table IR in unreachable
        // structured regions. The sole typed producer already checked their
        // concrete/Bot semantics; never pop a nonexistent physical SSA value.
        if(!control_stack.back().is_reachable) { return true; }
        return event.opcode == 0x25u ? emit_table_get_call(event.table_index, reference_value_type) :
            emit_table_set_call(event.table_index, reference_value_type);
    }
    else
    {
        // Pending migration: this legacy branch still decodes other raw opcode families.
        if(code_curr == code_end) [[unlikely]] { return false; }


    // Decode the opcode byte and record its function-relative offset for tiered
    // OSR and the adjacent immutable cast/get borrow, independently of debug.
    // [same pinned expression: begin ... code_curr ...] end
    // [safe                                           ] one-past
    // ^^ This is cold compiler metadata; no source pointer advances and no
    // additional memory-access instruction/guard is emitted into guest code.
    wasm1_code curr_opbase;
    ::std::memcpy(::std::addressof(curr_opbase), code_curr, sizeof(wasm1_code));
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
    // All unlisted validated opcodes retire BEFORE pop/call/native escape.
    // Pure numeric/local/control instructions retain entry ownership, but the
    // ORIGINAL byte-offset+empty-BB cast/get rule still invalidates raw borrows.
    auto const sealed_opcode{static_cast<::std::uint_least8_t>(curr_opbase)};
    bool const sealed_pure{sealed_opcode==0x01u || (sealed_opcode>=0x02u && sealed_opcode<=0x05u) ||
        (sealed_opcode>=0x0bu && sealed_opcode<=0x0eu) || sealed_opcode==0x1au ||
        (sealed_opcode>=0x20u && sealed_opcode<=0x22u) || sealed_opcode==0x25u || sealed_opcode==0x26u ||
        (sealed_opcode>=0x41u && sealed_opcode<=0xc4u) || sealed_opcode==0xfbu};
    if(control_stack.back().is_reachable && !sealed_pure &&
       (!snapshot_runtime_local_func_llvm_jit_gc_roots(state) || !sealed_compact_codegen::retire(state))) { return false; }
#endif
    if(state.local_func_storage_ptr != nullptr && state.local_func_storage_ptr->code_begin != nullptr && code_curr >= state.local_func_storage_ptr->code_begin &&
       code_curr < state.local_func_storage_ptr->code_end)
    {
        state.current_wasm_op_offset = static_cast<::std::size_t>(code_curr - state.local_func_storage_ptr->code_begin);
    }
    else
    {
        state.current_wasm_op_offset = SIZE_MAX;
    }

    if(!emit_runtime_local_func_llvm_jit_native_provenance(state, state.current_wasm_op_offset)) { return false; }

    if(state.pending_numeric_plan != nullptr &&
       !pending_numeric_primary_opcode(static_cast<unsigned>(curr_opbase))) { return false; }

    // Loop, else and end points belong to their post-PHI target blocks. Other
    // operations stop before consuming their inputs, including musttail calls.
    // Inline validation handlers may already have emitted this exact point.
    if(curr_opbase != wasm1_code::loop && curr_opbase != wasm1_code::else_ && curr_opbase != wasm1_code::end &&
       !emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) [[unlikely]] { return false; }

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
                // unreachable block/loop/if opcode ... code_end
                // [safe opcode] unsafe (could be code_end)
                // ^^ code_curr: the dispatcher read a live opcode; one-byte advance may reach one-past.
                ++code_curr;
                // unreachable block/loop/if opcode ... code_end
                // [safe opcode] unsafe (could be code_end)
                //               ^^ code_curr: subsequent immediate reads are bounded.
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
                // unreachable else opcode ... code_end
                // [safe opcode] unsafe (could be code_end)
                // ^^ code_curr: the dispatcher read a live opcode; one-byte advance may reach one-past.
                ++code_curr;
                // unreachable else opcode ... code_end
                // [safe opcode] unsafe (could be code_end)
                //               ^^ code_curr: subsequent immediate reads are bounded.
                    return code_curr == code_end;
                }
                break;
            }
            case wasm1_code::end:
            {
                if(unreachable_control_depth != 0uz)
                {
                // unreachable end opcode ... code_end
                // [safe opcode] unsafe (could be code_end)
                // ^^ code_curr: the dispatcher read a live opcode; one-byte advance may reach one-past.
                ++code_curr;
                // unreachable end opcode ... code_end
                // [safe opcode] unsafe (could be code_end)
                //               ^^ code_curr: subsequent immediate reads are bounded.
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

    if(static_cast<::std::uint_least8_t>(curr_opbase) == static_cast<::std::uint_least8_t>(wasm1p1_code::numeric_prefix))
    {
        // FC numeric prefix opcode ... code_end
        // [safe opcode] unsafe (could be code_end)
        // ^^ code_curr: the dispatcher read a live prefix; one-byte advance may reach one-past.
        ++code_curr;
        // FC numeric prefix opcode ... code_end
        // [safe opcode] unsafe (could be code_end)
        //               ^^ code_curr: subsequent immediate reads are bounded.

        validation_module_traits_t::wasm_u32 subopcode{};
        if(!parse_wasm_leb128_immediate(code_curr, code_end, subopcode)) [[unlikely]] { return false; }

        switch(static_cast<wasm1p1_numeric_code>(subopcode))
        {
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
            // Sole authoritative tableidx decoder now supplies table_access_event.
            // Raw replay refuses BEFORE reading/advancing any opcode/immediate.
            // [caller-owned original instruction] | end
            // [safe] ^^ code_curr is not read or moved in this refusal.
            return false;
        }
        case static_cast<::std::uint_least8_t>(wasm1p1_code::table_set):
        {
            // Sole authoritative tableidx decoder now supplies table_access_event.
            // Raw replay refuses BEFORE reading/advancing any opcode/immediate.
            // [caller-owned original instruction] | end
            // [safe] ^^ code_curr is not read or moved in this refusal.
            return false;
        }
        case static_cast<::std::uint_least8_t>(wasm1p1_code::ref_null):
        {
            // Owned normalized family: the authoritative typed dispatcher
            // alone decodes/validates the heap and then supplies ref_null_event.
            // A raw replay cannot recover that event or its source authority.
            // [original checked opcode ... immediate] | instruction_end
            // [safe opcode                           ] | one-past
            // ^^ code_curr: refuse without advancing or reading any heap byte.
            return false;
        }
        case 0xd4u:
        {
            // [ref.as_non_null] next opcode ... end
            // [safe           ] unsafe (could be end)
            // ^^ code_curr; instruction dispatch proved one byte available.
            ++code_curr;
            // [ref.as_non_null] next opcode ... end
            // [safe           ] unsafe (could be end)
            //                   ^^ code_curr: no immediate; the shared validator already checked the feature/type.
            if(!emit_ref_as_non_null()) [[unlikely]] { return false; }
            return code_curr == code_end;
        }

        case 0xd3u: // Core 3 ref.eq
        {
            // [ref.eq] next opcode ... end
            // [safe  ] unsafe (could be end)
            // ^^ code_curr: dispatcher proved the opcode byte.
            ++code_curr;
            // [ref.eq] next opcode ... end
            // [safe  ] unsafe (possibly one-past)
            //          ^^ code_curr: no immediate follows.
            if(operand_stack.size() < 2uz) [[unlikely]] { return false; }
            auto const lhs{operand_stack[operand_stack.size() - 2uz]};
            auto const rhs{operand_stack.back()};
            constexpr auto ref_bytes{sizeof(runtime_wasm_global_ref)};
            constexpr auto storage_bytes{sizeof(decltype(runtime_wasm_global_ref::storage))};
            constexpr auto storage_offset{offsetof(runtime_wasm_global_ref, storage)};
            constexpr auto i31_bytes{sizeof(::uwvm2::parser::wasm::standard::wasm3::type::wasm_i31)};
            static_assert(i31_bytes <= storage_bytes);
            constexpr auto tag_bytes{sizeof(decltype(runtime_wasm_global_ref::kind))};
            constexpr auto tag_offset{offsetof(runtime_wasm_global_ref, kind)};
            constexpr auto ref_bits{static_cast<unsigned>(ref_bytes * CHAR_BIT)};
            if(lhs.type != runtime_operand_stack_value_type::funcref ||
               rhs.type != runtime_operand_stack_value_type::funcref || lhs.value == nullptr || rhs.value == nullptr ||
               !lhs.value->getType()->isIntegerTy(ref_bits) || !rhs.value->getType()->isIntegerTy(ref_bits))
            { return false; }
            auto const little{llvm_module->getDataLayout().isLittleEndian()};
            auto const tag_shift{static_cast<unsigned>((little ? tag_offset : ref_bytes - tag_offset - tag_bytes) * CHAR_BIT)};
            auto const storage_shift{static_cast<unsigned>((little ? storage_offset :
                ref_bytes - storage_offset - storage_bytes) * CHAR_BIT)};
            auto const tag_type{ir_builder.getIntNTy(static_cast<unsigned>(tag_bytes * CHAR_BIT))};
            auto const storage_type{ir_builder.getIntNTy(static_cast<unsigned>(storage_bytes * CHAR_BIT))};
            auto const extract_tag{[&](::llvm::Value* ref)
            { return ir_builder.CreateTrunc(ir_builder.CreateLShr(ref, tag_shift), tag_type); }};
            auto const extract_storage{[&](::llvm::Value* ref)
            { return ir_builder.CreateTrunc(ir_builder.CreateLShr(ref, storage_shift), storage_type); }};
            auto const left_tag{extract_tag(lhs.value)};
            auto const right_tag{extract_tag(rhs.value)};
            auto const same_tag{ir_builder.CreateICmpEQ(left_tag, right_tag)};
            auto const null_tag{::llvm::ConstantInt::get(tag_type,
                static_cast<unsigned>(::uwvm2::object::global::wasm_ref_kind::wasm_null))};
            auto const i31_tag{::llvm::ConstantInt::get(tag_type,
                static_cast<unsigned>(::uwvm2::object::global::wasm_ref_kind::wasm_i31))};
            auto const is_null{ir_builder.CreateICmpEQ(left_tag, null_tag)};
            auto const is_i31{ir_builder.CreateICmpEQ(left_tag, i31_tag)};
            auto const left_storage{extract_storage(lhs.value)};
            auto const right_storage{extract_storage(rhs.value)};
            auto const all_equal{ir_builder.CreateICmpEQ(left_storage, right_storage)};
            // The i31 occupies the first four bytes of the union. On big-endian
            // targets those bytes are the high half of its pointer-width slot.
            auto const left_i31{little ? left_storage :
                ir_builder.CreateLShr(left_storage, static_cast<unsigned>((storage_bytes - i31_bytes) * CHAR_BIT))};
            auto const right_i31{little ? right_storage :
                ir_builder.CreateLShr(right_storage, static_cast<unsigned>((storage_bytes - i31_bytes) * CHAR_BIT))};
            // i31 has only 31 observable bits; the remaining union bytes are ignored.
            auto const i31_mask{::llvm::ConstantInt::get(storage_type, 0x7fff'ffffu)};
            auto const i31_equal{ir_builder.CreateICmpEQ(ir_builder.CreateAnd(left_i31, i31_mask),
                ir_builder.CreateAnd(right_i31, i31_mask))};
            auto const equal_payload{ir_builder.CreateSelect(is_i31, i31_equal, all_equal)};
            auto const equal{ir_builder.CreateAnd(same_tag, ir_builder.CreateOr(is_null, equal_payload))};
            auto const result_value{ir_builder.CreateZExt(equal, ir_builder.getInt32Ty(), get_llvm_string_ref(u8"ref.eq"))};
            // [SSA prefix][eqref][eqref] end -> [SSA prefix][i32] end.
            // [safe                        ] both operands copied before pop invalidates vector entries.
            operand_stack.pop_back();
            operand_stack.pop_back();
            push_operand(runtime_operand_stack_value_type::i32, result_value);
            return code_curr == code_end;
        }
        case static_cast<::std::uint_least8_t>(wasm1p1_code::ref_is_null):
        {
            // Owned normalized family: one authoritative typed pop supplies
            // ref_is_null_event to its direct original physical SSA primitive.
            // [original checked opcode] next ... | instruction_end
            // [safe                   ]         | one-past is never read.
            // ^^ code_curr: refuse raw replay without advance or byte access.
            return false;
        }
        case static_cast<::std::uint_least8_t>(wasm1p1_code::ref_func):
        {
            // Owned decoded family: C.funcs/C.refs and the exact declared heap
            // were checked during the sole first typed transition. Only its
            // ref_func_event enters the original physical reference primitive.
            // [original checked opcode] next ... | instruction_end
            // [safe                   ]         | one-past is never read.
            // ^^ code_curr: refuse raw replay without advance or immediate scan.
            return false;
        }
        case 0xfbu:
        {
            // [FB] subopcode ... instruction_end
            // [safe] unsafe (could be instruction_end)
            // ^^ code_curr: the enclosing nonempty-slice check proved the prefix.
            ++code_curr;
            // [FB] subopcode ... instruction_end
            // [safe] unsafe (could be instruction_end)
            //        ^^ code_curr: shared scanner checks the remaining bounded bytes.
            auto const decoded{::uwvm2::validation::standard::wasm3::scan_gc_instruction(code_curr, code_end)};
            if(decoded.error != ::uwvm2::validation::standard::wasm3::gc_immediate_error::ok ||
               code_curr != code_end) [[unlikely]] { return false; }
            // [FB][checked subopcode/immediate] instruction_end
            // [safe                           ] unsafe (one-past)
            //                                   ^^ code_curr: scanner committed only checked input.
            if(decoded.opcode <= 19u)
            { return try_emit_runtime_local_func_llvm_jit_gc_aggregate(state, decoded); }
            if(decoded.opcode <= 23u)
            {
                auto const* body{state.local_func_storage_ptr == nullptr ? nullptr :
                    state.local_func_storage_ptr->code_begin};
                auto const* body_end{state.local_func_storage_ptr == nullptr ? nullptr :
                    state.local_func_storage_ptr->code_end};
                if(body == nullptr || body_end == nullptr || code_end < body || code_end > body_end)
                { return false; }
                // [same validated function expression][cast slice] next opcode
                // [safe                                                    ]
                // The scanner consumed the WHOLE instruction slice above.
                // Only an integer byte offset is kept; no decoding cursor or
                // pointer advances, and a following nop/local/branch expires it.
                return try_emit_runtime_local_func_llvm_jit_gc_ref_test_cast(state, decoded,
                    static_cast<::std::size_t>(code_end - body));
            }
            if(decoded.opcode <= 25u)
            { return try_emit_runtime_local_func_llvm_jit_gc_br_on_cast(state, decoded); }
            if(decoded.opcode <= 27u)
            { return try_emit_runtime_local_func_llvm_jit_gc_convert(state, decoded); }
            // Scalar-reference GC 28..30 require the fused scanner's original
            // owned decoded DATA. This legacy raw path cannot authorize their lowering.
            return false;
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
            // unreachable opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            // ^^ code_curr: the dispatcher read a live opcode; one-byte advance may reach one-past.
            ++code_curr;
            // unreachable opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            //               ^^ code_curr: subsequent immediate reads are bounded.
            if(!try_emit_runtime_local_func_llvm_jit_unreachable(state)) [[unlikely]] { return false; }
            break;
        }
        case wasm1_code::nop:
        {
            // nop opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            // ^^ code_curr: the dispatcher read a live opcode; one-byte advance may reach one-past.
            ++code_curr;
            // nop opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            //               ^^ code_curr: subsequent immediate reads are bounded.
            if(!try_emit_runtime_local_func_llvm_jit_nop(state)) [[unlikely]] { return false; }
            break;
        }
        case wasm1_code::block:
        {
            // block opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            // ^^ code_curr: the dispatcher read a live opcode; one-byte advance may reach one-past.
            ++code_curr;
            // block opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            //               ^^ code_curr: subsequent immediate reads are bounded.

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
            // loop opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            // ^^ code_curr: the dispatcher read a live opcode; one-byte advance may reach one-past.
            ++code_curr;
            // loop opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            //               ^^ code_curr: subsequent immediate reads are bounded.

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
            // if opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            // ^^ code_curr: the dispatcher read a live opcode; one-byte advance may reach one-past.
            ++code_curr;
            // if opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            //               ^^ code_curr: subsequent immediate reads are bounded.

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
            // else opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            // ^^ code_curr: the dispatcher read a live opcode; one-byte advance may reach one-past.
            ++code_curr;
            // else opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            //               ^^ code_curr: subsequent immediate reads are bounded.
            if(!try_emit_runtime_local_func_llvm_jit_else(state)) [[unlikely]] { return false; }
            break;
        }
        case wasm1_code::end:
        {
            // end opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            // ^^ code_curr: the dispatcher read a live opcode; one-byte advance may reach one-past.
            ++code_curr;
            // end opcode ... code_end
            // [safe opcode] unsafe (could be code_end)
            //               ^^ code_curr: subsequent immediate reads are bounded.
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
}

[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_instruction(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::byte const* begin, ::std::byte const* end) noexcept
{ return try_emit_runtime_local_func_llvm_jit_instruction_impl<false>(state, begin, end, nullptr); }

[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_scalar_memory(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_scalar_memory_event const& event) noexcept
{
    // [owned checked event] no raw source pointer is accepted or read by this entry.
    return try_emit_runtime_local_func_llvm_jit_instruction_impl<true>(state, nullptr, nullptr, ::std::addressof(event));
}

[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_memory_page(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_memory_page_event const& event) noexcept
{
    // [owned checked event] no raw source pointer is accepted or read by this entry.
    return try_emit_runtime_local_func_llvm_jit_instruction_impl<false, true>(
        state, nullptr, nullptr, nullptr, ::std::addressof(event));
}

[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_simd(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_simd_event const& event) noexcept
{
    // [owned checked event] no raw source slice is accepted by this entry.
    return try_emit_runtime_local_func_llvm_jit_instruction_impl<false, false, true>(state, nullptr, nullptr, nullptr, nullptr, ::std::addressof(event));
}

[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_atomic(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_atomic_event const& event) noexcept
{
    // [owned checked event] no raw source slice is accepted by this entry.
    return try_emit_runtime_local_func_llvm_jit_instruction_impl<false, false, false, true>(
        state, nullptr, nullptr, nullptr, nullptr, nullptr, ::std::addressof(event));
}

[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_bulk_memory(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_bulk_memory_event const& event) noexcept
{
    // The entry accepts only owned checked DATA, never a raw expression slice.
    return try_emit_runtime_local_func_llvm_jit_instruction_impl<false, false, false, false, true>(
        state, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, ::std::addressof(event));
}

[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_i32_numeric(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_i32_numeric_event const& event) noexcept
{
    // [same owned typed event] synchronous compiler borrow only.
    // [safe                 ] ^^ addressof copies the event address; there is
    // no source span, raw opcode read, native-PC or execution permission.
    return try_emit_runtime_local_func_llvm_jit_instruction_impl<false, false, false, false, false, true>(
        state, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, ::std::addressof(event));
}

[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_i64_numeric(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_i64_numeric_event const& event) noexcept
{
    // [same accepted owned DATA] no original body cursor, reread or matcher.
    // [safe synchronous borrow] ^^ event lifetime spans this compiler call only.
    return try_emit_runtime_local_func_llvm_jit_instruction_impl<false, false, false, false, false, false, true>(
        state, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, ::std::addressof(event));
}

[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_integer_width(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_integer_width_event const& event) noexcept
{
    // [same owned first-typing DATA] synchronous borrow; no raw body/cursor read.
    // [safe] event's lifetime covers this compiler call, not execution permission.
    return try_emit_runtime_local_func_llvm_jit_instruction_impl<false, false, false, false, false, false, false, true>(
        state, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, ::std::addressof(event));
}

[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_integer_compare(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_integer_compare_event const& event) noexcept
{
    // [same owned first-typing DATA] end; no raw body/cursor accepted.
    // [safe synchronous borrow] ^^ lifetime covers this compiler call only.
    return try_emit_runtime_local_func_llvm_jit_instruction_impl<false, false, false, false, false, false, false, false, true>(
        state, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, ::std::addressof(event));
}

[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_table_access(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_table_access_event const& event) noexcept
{
    // [same complete first-typed table DATA] end; no raw source range accepted.
    // [safe synchronous borrow] ^^ lifetime lasts through this cold compiler call.
    return try_emit_runtime_local_func_llvm_jit_instruction_impl<false, false, false, false, false, false, false, false, false, true>(
        state, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, ::std::addressof(event));
}

#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_HOST_ADDRESS_CARRIER")
