// Included in the real runtime TU namespace, after its exact anonymous record
// forward declarations. No exported factory, serialized credential or hot path.
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
namespace details
{
    struct native_owner_publication_bridge final
    {
        using pending=::uwvm2::runtime::lib::details::pending_actual_native_endpoints;
        class captured final
        {
            friend struct native_owner_publication_bridge;
            ::uwvm2::uwvm::runtime::full::full_source_instance::owner source_{};
            compiled_module_record const* record_{}; // comparison only until real registry selected
            full_code_publication const* publication_{};
            llvm_jit_debug_replace_transaction const* replacement_{};
            ::std::uintptr_t expression_begin_{},expression_end_{};
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* module_{};
            ::std::size_t module_id_{SIZE_MAX};
            ::std::uint_least64_t epoch_{};
            ::std::vector<pending::expected_function> expected_{};
            captured()=default;
        public:
            captured(captured const&)=delete;
            captured& operator=(captured const&)=delete;
        };
        // Called ONLY at the actual optimized IR handoff seam, before parallel
        // object emission or reset, under the already-held publication guard.
        [[nodiscard]] static ::std::unique_ptr<captured> capture_actual_full(
            compiled_module_record&,::llvm::Module&,::llvm::LLVMContext const&) noexcept;
        [[nodiscard]] static ::std::unique_ptr<pending> observe_actual_full(
            ::llvm::ExecutionEngine&,captured const&) noexcept;
        [[nodiscard]] static bool seal_actual_full(
            compiled_module_record&,captured const&,pending&) noexcept;
        [[nodiscard]] static ::std::unique_ptr<captured> capture_actual_replacement(compiled_module_record&,
            llvm_jit_debug_replace_transaction&,::llvm::Module&,::llvm::LLVMContext const&,
            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::local_func_storage_t const&) noexcept;
        [[nodiscard]] static ::std::unique_ptr<pending> observe_actual_replacement(
            llvm_jit_debug_replace_transaction&,captured const&) noexcept;
        [[nodiscard]] static bool seal_actual_prepared_replacement(
            llvm_jit_debug_replace_transaction&,captured const&,pending&) noexcept;
        [[nodiscard]] static bool qualifies_actual_replacement_for_commit(
            compiled_module_record const&,llvm_jit_debug_replace_transaction const&) noexcept;
        // Internal DATA join only, never a public permission. Actual consumers
        // must retain their authenticated stopped participant/cursor/cohort and
        // execution/GC lease BEFORE this publication-locked exact lookup.
        [[nodiscard]] static bool resolve_actual_full(compiled_module_record const&,
            ::std::size_t,::std::uint_least64_t,::std::uintptr_t,::std::uintptr_t&,::std::uintptr_t&) noexcept;
    };
}
#endif
