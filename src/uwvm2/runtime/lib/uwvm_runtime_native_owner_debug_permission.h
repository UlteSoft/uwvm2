// Runtime-private cold permission join. Include only after the actual native
// endpoint publication bridge definition in the real runtime implementation.
// This function takes no code pointer from a public debugger command. Its
// callers retain the real generated worker lease or a canonical stopped
// capture + GC/engine lease + ONE actual parked-participant transaction.
#if defined(UWVM_RUNTIME_LLVM_JIT)
    namespace
    {
        [[nodiscard]] inline bool debug_resolve_actual_native_function_body(
            ::std::uint_least64_t module, ::std::uint_least64_t function,
            ::std::uint_least64_t generation, ::std::uint_least64_t epoch,
            ::std::uintptr_t pc, ::std::uintptr_t& begin, ::std::uintptr_t& end) noexcept
        {
            begin = end = 0u;
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
            if(get_runtime_state_publication_depth() != 1u || epoch == 0u || epoch != current_runtime_generation() ||
               generation == 0u || pc == 0u || module >= g_runtime.modules.size() || function > SIZE_MAX)
            { return false; }
            // [actual publication-pinned module table 0 ... modules.size)
            // [safe] module checked BEFORE selecting its real owned record.
            // This checked record is then independently canonicalized against
            // the bridge's actual registry; no saved opaque pointer is cast.
            auto const& record{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(module))};
            if(!details::native_owner_publication_bridge::resolve_actual_full(record,
                static_cast<::std::size_t>(function), generation, pc, begin, end)) { return false; }
            if(begin == 0u || end <= begin || pc < begin || pc >= end)
            { begin = end = 0u; return false; }
            return true;
#else
            // A missing patched producer, unknown object shape, source owner or
            // restored epoch never falls back to diagnostic computeSymbolSizes.
            // The all-platform endpoint source remains unqualified until the
            // actual SDK/object-load/fresh-runtime path has been tested.
            (void)module; (void)function; (void)generation; (void)epoch; (void)pc;
            return false;
#endif
        }
    }
#endif
