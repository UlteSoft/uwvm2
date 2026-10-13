#pragma once
// Included in the existing LLVM bridge namespace after the native and i32
// provider bulk bridges. These cold host-provider entry points accept only
// translator-owned object relocations; Wasm operands remain unsigned offsets.

inline void llvm_jit_local_imported_memory64_fill_bridge(
    ::std::uintptr_t provider_address, ::std::size_t memory_index,
    ::std::uint64_t destination, runtime_wasm_i32 value,
    ::std::uint64_t length) noexcept
{
    namespace wide = ::uwvm2::runtime::compiler::shared::wasm_memory64;
    // [live type-erased provider] retained by the execution lease.
    // [safe                    ] provider_address is a trusted relocation;
    //  ^^ no Wasm integer is ever promoted to a native object address.
    auto const provider{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(provider_address)};
    ::std::size_t memory_length{};
    if(!llvm_jit_local_imported_memory_byte_length(provider, memory_index, memory_length) ||
       !wide::range_valid(memory_length, destination, length)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap(memory_index, 0u, {destination, false},
            memory_length, llvm_jit_bulk_diagnostic_length(length));
    }
    // Zero length still passes the complete full-width range proof above.
    if(length == 0u) { return; }

    constexpr ::std::size_t capacity{4096uz};
    ::std::byte staging[capacity]{};
    ::std::memset(staging, static_cast<int>(static_cast<unsigned char>(llvm_jit_wasm_i32_bits_to_u32(value))), capacity);
    // range_valid proves length <= memory_length <= SIZE_MAX before narrowing.
    auto const native_length{static_cast<::std::size_t>(length)};
    ::std::size_t filled{};
    while(filled != native_length)
    {
        auto const remaining{native_length - filled};
        auto const chunk{remaining < capacity ? remaining : capacity};
        // [complete provider range: destination ... destination+length]
        // [safe                 ] filled < length and chunk <= length-filled;
        //  ^^ advancing the provider OFFSET cannot overflow or exceed the range.
        // [owned staging: 0 ... chunk ... capacity] no pointer is advanced.
        if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_write(
               provider, memory_index, destination + static_cast<::std::uint64_t>(filled), staging, chunk)) [[unlikely]]
        {
            llvm_jit_memory_bridge_trap(memory_index, 0u, {destination, false},
                memory_length, llvm_jit_bulk_diagnostic_length(length));
        }
        // chunk <= native_length-filled proves the next integer offset is bounded.
        filled += chunk;
    }
}

inline void llvm_jit_local_imported_memory64_init_bridge(
    ::std::uintptr_t provider_address, ::std::size_t memory_index,
    ::std::uintptr_t module_address, ::std::uint64_t data_index,
    ::std::uint64_t destination, llvm_jit_atomic_address_carrier_t source_carrier,
    llvm_jit_atomic_address_carrier_t length_carrier) noexcept
{
    namespace wide = ::uwvm2::runtime::compiler::shared::wasm_memory64;
    // [live provider/module] pinned by the same compiled execution lease.
    // [safe               ] both object addresses are trusted relocations.
    auto const provider{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(provider_address)};
    auto const module{reinterpret_cast<runtime_module_storage_t*>(module_address)};
    auto const source{static_cast<::std::uint32_t>(source_carrier)};
    auto const length{static_cast<::std::uint32_t>(length_carrier)};
    if(provider == nullptr || module == nullptr || data_index >= module->local_defined_data_vec_storage.size()) [[unlikely]]
    { llvm_jit_memory_bridge_trap(); }

    // [owned data vector: 0 ... data_index ... size] full-width comparison
    // [safe                                    ] precedes native narrowing/access.
    auto const& data{module->local_defined_data_vec_storage.index_unchecked(static_cast<::std::size_t>(data_index)).data};
    auto const payload{::uwvm2::uwvm::runtime::storage::load_wasm_data_segment_payload(data)};
    if((payload.byte_begin == nullptr) != (payload.byte_end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
    // [immutable module payload ...] end, or two null pointers after data.drop.
    // [safe                       ] begin/end share one owned allocation;
    //  ^^ no subtraction is evaluated for the null, dropped representation.
    auto const source_length{payload.byte_begin == nullptr ? 0uz :
        static_cast<::std::size_t>(payload.byte_end - payload.byte_begin)};
    if(!wide::range_valid(source_length, source, length)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap(memory_index, 0u, {source, false}, source_length, length);
    }

    ::std::size_t memory_length{};
    if(!llvm_jit_local_imported_memory_byte_length(provider, memory_index, memory_length) ||
       !wide::range_valid(memory_length, destination, length)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap(memory_index, 0u, {destination, false}, memory_length, length);
    }
    if(length == 0u) { return; }

    // Both complete ranges were proven before the first provider write.
    // [immutable payload: 0 ... source ... source+length ... source_length]
    // [safe                                                           ]
    //  ^^ adding source stays inside the allocation because length>0.
    // A dropped snapshot has length zero, so the proven nonempty path has begin.
    if(payload.byte_begin == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const source_begin{payload.byte_begin + static_cast<::std::size_t>(source)};
    if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_write(
           provider, memory_index, destination, source_begin, static_cast<::std::size_t>(length))) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap(memory_index, 0u, {destination, false}, memory_length, length);
    }
}
