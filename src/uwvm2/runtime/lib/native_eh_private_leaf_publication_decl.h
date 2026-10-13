// Original runtime anonymous namespace. Exact-1 candidate only: the actual
// full materializer is the sole friend that may create/seal this native owner.
class owned_native_eh_private_leaf_publication final
{
    friend inline constexpr bool try_materialize_runtime_module_llvm_jit(
        compiled_module_record&, bool, ::llvm::CodeGenOptLevel, ::std::size_t) noexcept;
    using compiler_type = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::full_function_symbol_t;
    using source_type = ::uwvm2::uwvm::runtime::full::full_source_instance;
    using stage_type = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::native_eh_private_leaf::staged_module;
    struct loaded_clone { ::std::size_t local_index{}; ::std::uintptr_t begin{}, size{}; };
    source_type::owner source_{};
    ::std::shared_ptr<stage_type const> stage_{};
    ::std::uint_least64_t runtime_generation_{};
    ::std::size_t module_id_{SIZE_MAX}, clone_count_{};
    loaded_clone loaded_[256uz]{};
    ::llvm::ExecutionEngine* actual_engine_{}; // Borrow tied to containing full_code_publication.
    bool loaded_binding_{}, committed_{};
    // Original public IR/context stay owned until candidate publication commits;
    // private engine failure restores this exact ordinary product, not stage IR.
    ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::llvm_jit_module_storage_t original_{};
    owned_native_eh_private_leaf_publication(source_type::owner source,
        ::std::shared_ptr<stage_type const> stage, ::std::size_t module_id, ::std::uint_least64_t generation) noexcept
        : source_{::std::move(source)}, stage_{::std::move(stage)}, runtime_generation_{generation}, module_id_{module_id} {}
    [[nodiscard]] static ::uwvm2::utils::container::delete_owned_ptr<owned_native_eh_private_leaf_publication>
        prepare_actual_record(compiled_module_record& rec) noexcept;
    [[nodiscard]] bool bind_actual_numeric_bridge(::llvm::ExecutionEngine& engine, ::llvm::LLVMContext& context) noexcept;
    [[nodiscard]] bool verify_actual_selected_calls(compiled_module_record const& rec, ::llvm::ExecutionEngine& engine) const noexcept;
    [[nodiscard]] bool seal_actual_loaded(compiled_module_record const& rec, ::llvm::ExecutionEngine& engine,
        ::uwvm2::runtime::lib::details::pending_llvm_jit_code_ranges const& ranges,
        ::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager const& manager) noexcept;
    void restore_original_after_failed_private_engine(compiled_module_record& rec) noexcept;
    void commit_original_retirement() noexcept
    {
        // Actual native engine/CFI/ranges/publication have committed already.
        // Original untouched public fallback now retires in dependency order.
        original_.llvm_module.reset(); original_.llvm_context_holder.reset();
        committed_ = true;
    }
public:
    // Read-only native publication observation, never an attach/replacement
    // permission. The host registry/lease must retain this actual owner.
    [[nodiscard]] bool actual_loaded_binding_matches(compiled_module_record const& rec) const noexcept;
    [[nodiscard]] ::std::size_t actual_loaded_clone_count() const noexcept { return clone_count_; }
    owned_native_eh_private_leaf_publication(owned_native_eh_private_leaf_publication const&) = delete;
    owned_native_eh_private_leaf_publication& operator=(owned_native_eh_private_leaf_publication const&) = delete;
    ~owned_native_eh_private_leaf_publication() = default;
};
