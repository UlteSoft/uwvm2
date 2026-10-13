// TU-private compiled root identity. A prepared identity owns immutable DATA;
// only the real current materializer can seal it after actual source/native adoption.
// A generation integer, copied epoch or staged listener never seals this owner.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
class checkpoint_root_generation_identity final
{
    friend struct checkpoint_root_generation_publication_bridge;
    using source_owner=::uwvm2::uwvm::runtime::full::full_source_instance::owner;
    using profile_owner=::uwvm2::runtime::checkpoint::compilation_profile::owner;
    struct row
    {
        ::uwvm2::runtime::checkpoint::sealed_function_plan::owner plan{};
        void const* wasm_code{};
        ::std::uintptr_t expression_begin{},expression_end{},typed{},raw{},resume{};
        ::std::uint64_t generation{};
    };
    source_owner source_{};
    profile_owner profile_{};
    void const* module_{};
    ::llvm::ExecutionEngine const* engine_{};
    ::llvm::LLVMContext const* context_{};
    full_code_publication const* publication_{}; // comparison only, owned by this publication
    ::std::size_t module_id_{SIZE_MAX};
    ::std::vector<row> rows_{};
    ::std::uint_least64_t sealed_epoch_{},initializer_serial_{};
    checkpoint_root_generation_identity()=default;
public:
    checkpoint_root_generation_identity(checkpoint_root_generation_identity const&)=delete;
    checkpoint_root_generation_identity& operator=(checkpoint_root_generation_identity const&)=delete;
};
struct checkpoint_root_generation_publication_bridge final
{
    friend class ::uwvm2::runtime::lib::runtime_checkpoint_world_transaction;
    friend inline constexpr bool try_materialize_runtime_module_llvm_jit(compiled_module_record&,
        bool,::llvm::CodeGenOptLevel,::std::size_t) noexcept;
private:
    using identity=checkpoint_root_generation_identity;
    template<typename A,typename B>
    [[nodiscard]] static bool same_owner(A const& a,B const& b) noexcept
    { return a && b && a.get()==b.get() && !a.owner_before(b) && !b.owner_before(a); }
    [[nodiscard]] static bool matches_record(identity const&,compiled_module_record const&,
        ::std::size_t,bool) noexcept;
    [[nodiscard]] static bool matches_row(identity const&,compiled_module_record const&,::std::size_t,bool) noexcept;
    [[nodiscard]] static ::std::unique_ptr<identity> prepare(compiled_module_record const&,::std::size_t) noexcept;
    [[nodiscard]] static bool seal_actual_full(compiled_module_record&) noexcept;
    [[nodiscard]] static bool matches_private_prepared(compiled_module_record const&,::std::size_t) noexcept;
    [[nodiscard]] static constexpr ::std::size_t allocation_bytes() noexcept { return sizeof(identity); }
    [[nodiscard]] static constexpr ::std::size_t row_bytes() noexcept { return sizeof(identity::row); }
public:
    // Internal exact DATA lookup; caller still needs its real execution or
    // stopped/publication scope. No native read/step grant is issued here.
    [[nodiscard]] static bool matches_current_root(compiled_module_record const&,::std::size_t,::std::uint64_t) noexcept;
};
#endif
