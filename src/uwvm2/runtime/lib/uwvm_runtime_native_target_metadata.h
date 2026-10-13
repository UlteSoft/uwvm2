// Runtime-private actual native decoder description. Include only after the
// real activation/code-owner helpers and complete full/replacement publishers.
// No ordinary publication, TLS entry or generated Wasm operation calls this.
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
namespace
{
    template<typename Array, typename String>
    [[nodiscard]] bool debug_copy_native_target_string(Array& out, ::std::size_t& count, String const& actual) noexcept
    {
        auto const size{actual.size()};
        if(size >= out.size()) { return false; }
        for(::std::size_t index{}; index != size; ++index)
        {
            // [actual engine-owned complete StringRef ... size) end
            // [safe                                              ] index<size;
            //  ^^ reject embedded NUL before supplying a C decoder string.
            if(actual[index] == '\0') { return false; }
        }
        // [actual bounded source ... size) [owned array capacity > size]
        // [safe                         ] zero size performs no borrowed read;
        //  ^^ engine/code-owner lease remains live through this synchronous copy.
        if(size != 0u) { ::fast_io::freestanding::my_memcpy(out.data(), actual.data(), size); }
        out[size] = '\0'; count = size;
        return true;
    }
    // LLVM21 returns nullable SDK pointers; pinned LLVM23 returns actual
    // references. Normalize ONLY this same leased real TargetMachine's values.
    // A nullable old-SDK provider must be checked BEFORE any dereference.
    template<typename Value>
    [[nodiscard]] auto const* debug_native_sdk_address(Value const& actual) noexcept
    {
        if constexpr(::std::is_pointer_v<Value>) { return actual; }
        else { return ::std::addressof(actual); }
    }
    template<typename Assembly, typename Subtarget, typename Triple>
    [[nodiscard]] unsigned debug_native_max_instruction_bytes(Assembly const& assembly, Subtarget const& subtarget,
        Triple const& actual_triple) noexcept
    {
        // LLVM23 X86 MCAsmInfo inherits the inline-assembly estimate of four
        // bytes. It is not an X86 decoder bound: genuine instructions can be
        // fifteen bytes. Select the architectural bound from THIS leased
        // TargetMachine's actual triple, never the host or a requested target.
        // The decoder still borrows only the bounded owned function copy.
        if(actual_triple.isX86()) { return 15u; }
        if constexpr(requires { assembly.getMaxInstLength(::std::addressof(subtarget)); })
        { return assembly.getMaxInstLength(::std::addressof(subtarget)); }
        else { return assembly.getMaxInstLength(); }
    }
    [[nodiscard]] bool debug_copy_actual_native_target(::std::uint_least64_t module_id,
        ::std::uint_least64_t function, ::std::uint_least64_t generation, void const* expected_owner,
        llvm_jit_debug_native_target& out) noexcept
    {
        out = {};
        if(expected_owner == nullptr || module_id >= g_runtime.modules.size()) { return false; }
        auto const& record{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(module_id))};
        auto const* module{record.runtime_module};
        if(module == nullptr) { return false; }
        auto const imports{module->imported_function_vec_storage.size()};
        if(function < imports || function - imports >= record.llvm_jit_debug_full_entry_generations.size()) { return false; }
        auto const local{static_cast<::std::size_t>(function - imports)};
        if(debug_activation_current_code_owner(record, module_id, function, local, generation) != expected_owner) { return false; }
        ::llvm::ExecutionEngine* engine{};
        if(generation == 1u) { engine = record.llvm_jit_full_publication->engine.get(); }
        else
        {
            for(auto const& retained : record.llvm_jit_debug_full_retained_generations)
            {
                // [actual typed retained owners] publication/lease pins them
                // [safe                        ] compare the authentic token
                //  ^^ before borrowing this real generation's engine; no void* cast.
                if(retained && retained.get() == expected_owner) { engine = retained->engine.get(); break; }
            }
        }
        if(engine == nullptr) { return false; }
        auto const* machine{engine->getTargetMachine()};
        if(machine == nullptr) { return false; }
        auto const& layout{engine->getDataLayout()};
        auto const* assembly{debug_native_sdk_address(machine->getMCAsmInfo())};
        auto const* subtarget{debug_native_sdk_address(machine->getMCSubtargetInfo())};
        if(assembly == nullptr || subtarget == nullptr) { return false; }
#if UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE && defined(__arm__)
        // This software backend patches ARM-state four-byte BKPT instructions.
        // The actual JIT may differ from the host compiler's ISA state; refuse
        // Thumb before granting a plan, rather than overwrite a Thumb successor.
        if(machine->getTargetTriple().isThumb() || subtarget->checkFeatures("+thumb-mode")) { return false; }
#endif
        // [actual engine/TargetMachine-owned SDK providers] real lease/guard
        // [safe                                         ] both nonnull BEFORE
        //  ^^ synchronous dereference; no requested target or owner is substituted.
        auto const maximum{debug_native_max_instruction_bytes(*assembly, *subtarget, machine->getTargetTriple())};
        auto const alignment{assembly->getMinInstAlignment()};
        if(layout.isDefault() || layout.getPointerSizeInBits() != sizeof(::std::uintptr_t) * ::std::numeric_limits<unsigned char>::digits ||
           assembly->isLittleEndian() != layout.isLittleEndian() ||
           maximum == 0u || maximum > 32u || alignment == 0u || alignment > maximum ||
           (alignment & (alignment - 1u)) != 0u) { return false; }
        llvm_jit_debug_native_target candidate{};
        if(!debug_copy_native_target_string(candidate.triple, candidate.triple_size, machine->getTargetTriple().str()) ||
           candidate.triple_size == 0u ||
           !debug_copy_native_target_string(candidate.cpu, candidate.cpu_size, machine->getTargetCPU()) ||
           !debug_copy_native_target_string(candidate.features, candidate.features_size, machine->getTargetFeatureString()))
        { return false; }
        candidate.description_version = 1u;
        candidate.pointer_bits = layout.getPointerSizeInBits();
        candidate.little_endian = layout.isLittleEndian();
        candidate.maximum_instruction_bytes = maximum;
        candidate.minimum_instruction_alignment = alignment;
        candidate.available = true;
        out = candidate; return true;
    }
}
#endif
