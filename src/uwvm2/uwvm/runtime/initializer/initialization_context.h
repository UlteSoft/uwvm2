// Include only inside initializer::details, after actual storage/import/limit types.
#pragma once
// Native context DATA borrows, not a parser/store/publication/restore credential.
// Ordinary instantiations keep existing dynamic selected-registry lookup exactly.
extern ::uwvm2::utils::container::u8string_view current_initializing_module_name;
extern bool import_alias_sanity_checked;

enum class initialization_purpose : unsigned char { ordinary, unpublished_restore };

template <initialization_purpose Purpose>
class initialization_context final
{
    static_assert(Purpose == initialization_purpose::unpublished_restore);
    friend class ::uwvm2::uwvm::runtime::initializer::restoration_context;
    using registry = ::uwvm2::uwvm::runtime::storage::runtime_registry_type;
    using declaration_map = decltype(::uwvm2::uwvm::wasm::storage::all_module);
    using export_map = decltype(::uwvm2::uwvm::wasm::storage::all_module_export);
    using reset_map = ::uwvm2::uwvm::wasm::storage::configured_module_import_reset_map_t;
    using memory_map = ::uwvm2::uwvm::wasm::storage::configured_module_memory_limit_map_t;
    registry* registry_{};
    declaration_map* declarations_{};
    export_map* exports_{};
    reset_map* resets_{};
    memory_map* memory_limits_{};
    ::uwvm2::utils::container::u8string_view* current_{};
    bool* sanity_{};
    initializer_limit_t const* limits_{};
    ::uwvm2::uwvm::runtime::initializer::restoration_context* restoration_{};
    constexpr initialization_context() noexcept requires(Purpose == initialization_purpose::unpublished_restore) = default;
    explicit initialization_context(registry& modules, declaration_map& modules_decl,
        export_map& exports, reset_map& resets, memory_map& memories,
        ::uwvm2::utils::container::u8string_view& current, bool& sanity,
        initializer_limit_t const& caps,
        ::uwvm2::uwvm::runtime::initializer::restoration_context* restoration) noexcept
        requires(Purpose == initialization_purpose::unpublished_restore)
        : registry_{::std::addressof(modules)}, declarations_{::std::addressof(modules_decl)},
          exports_{::std::addressof(exports)}, resets_{::std::addressof(resets)},
          memory_limits_{::std::addressof(memories)}, current_{::std::addressof(current)},
          sanity_{::std::addressof(sanity)}, limits_{::std::addressof(caps)},
          restoration_{restoration} {}
public:
    constexpr initialization_context() noexcept requires(Purpose == initialization_purpose::ordinary) = default;
    initialization_context(initialization_context const&) = delete;
    initialization_context& operator=(initialization_context const&) = delete;
    [[nodiscard]] constexpr registry& modules() const noexcept
    {
        if constexpr(Purpose == initialization_purpose::ordinary)
        { return ::uwvm2::uwvm::runtime::storage::active_runtime_registry(); }
        else { return *registry_; }
    }
    [[nodiscard]] constexpr auto& declarations() const noexcept
    {
        if constexpr(Purpose == initialization_purpose::ordinary)
        { return ::uwvm2::uwvm::wasm::storage::all_module; }
        else { return *declarations_; }
    }
    [[nodiscard]] constexpr auto& exports() const noexcept
    {
        if constexpr(Purpose == initialization_purpose::ordinary)
        { return ::uwvm2::uwvm::wasm::storage::all_module_export; }
        else { return *exports_; }
    }
    [[nodiscard]] constexpr reset_map& import_reset_policy() const noexcept
    {
        if constexpr(Purpose == initialization_purpose::ordinary)
        { return ::uwvm2::uwvm::wasm::storage::configured_module_import_reset; }
        else { return *resets_; }
    }
    [[nodiscard]] constexpr memory_map& memory_limit_policy() const noexcept
    {
        if constexpr(Purpose == initialization_purpose::ordinary)
        { return ::uwvm2::uwvm::wasm::storage::configured_module_memory_limit; }
        else { return *memory_limits_; }
    }
    [[nodiscard]] constexpr auto& current_module() const noexcept
    {
        if constexpr(Purpose == initialization_purpose::ordinary) { return current_initializing_module_name; }
        else { return *current_; }
    }
    [[nodiscard]] constexpr bool& alias_sanity() const noexcept
    {
        if constexpr(Purpose == initialization_purpose::ordinary) { return import_alias_sanity_checked; }
        else { return *sanity_; }
    }
    [[nodiscard]] constexpr initializer_limit_t const& limits() const noexcept
    {
        if constexpr(Purpose == initialization_purpose::ordinary) { return initializer_limit; }
        else { return *limits_; }
    }
    [[nodiscard]] constexpr auto* find_import_reset(::uwvm2::utils::container::u8string_view name) const noexcept
    {
        auto& policy{import_reset_policy()}; auto const found{policy.find(name)};
        return found == policy.end() ? nullptr : ::std::addressof(found->second);
    }
    [[nodiscard]] constexpr ::uwvm2::uwvm::wasm::storage::configured_import_reset_t const*
        find_import_rewrite(::uwvm2::utils::container::u8string_view name,
            ::uwvm2::utils::container::u8string_view imported_module,
            ::uwvm2::utils::container::u8string_view imported_name) const noexcept
    {
        auto const* rules{find_import_reset(name)};
        if(rules == nullptr) { return nullptr; }
        // [source-owned bounded rule vector] end
        // [safe] compare native string views only; returned rule lives with source.
        for(auto const& rule : *rules)
        {
            if(rule.import_module_name == imported_module && rule.import_extern_name == imported_name)
            { return ::std::addressof(rule); }
        }
        return nullptr;
    }
    [[nodiscard]] constexpr auto const* find_memory_limit(::uwvm2::utils::container::u8string_view name) const noexcept
    {
        auto const& policy{memory_limit_policy()}; auto const found{policy.find(name)};
        return found == policy.cend() ? nullptr : ::std::addressof(found->second);
    }
    [[nodiscard]] ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store> make_store(
        ::uwvm2::uwvm::runtime::storage::gc_type::recursive_type_section const& section,
        ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_lease_owner> const& leases,
        ::uwvm2::uwvm::runtime::storage::gc_function_type_match_callback callback) const noexcept(Purpose == initialization_purpose::ordinary);
};

// The ordinary specialization is EMPTY: no staged pointers, member zeroing,
// source pins or runtime purpose branch can enter existing initialization/GC callbacks.
template<>
class initialization_context<initialization_purpose::ordinary> final
{
public:
    constexpr initialization_context() noexcept = default;
    [[nodiscard]] static constexpr auto& modules() noexcept
    { return ::uwvm2::uwvm::runtime::storage::active_runtime_registry(); }
    [[nodiscard]] static constexpr auto& declarations() noexcept
    { return ::uwvm2::uwvm::wasm::storage::all_module; }
    [[nodiscard]] static constexpr auto& exports() noexcept
    { return ::uwvm2::uwvm::wasm::storage::all_module_export; }
    [[nodiscard]] static constexpr auto& import_reset_policy() noexcept
    { return ::uwvm2::uwvm::wasm::storage::configured_module_import_reset; }
    [[nodiscard]] static constexpr auto& memory_limit_policy() noexcept
    { return ::uwvm2::uwvm::wasm::storage::configured_module_memory_limit; }
    [[nodiscard]] static constexpr auto& current_module() noexcept
    { return current_initializing_module_name; }
    [[nodiscard]] static constexpr bool& alias_sanity() noexcept
    { return import_alias_sanity_checked; }
    [[nodiscard]] static constexpr initializer_limit_t const& limits() noexcept
    { return initializer_limit; }
    [[nodiscard]] static constexpr auto* find_import_reset(::uwvm2::utils::container::u8string_view name) noexcept
    { return ::uwvm2::uwvm::wasm::storage::find_configured_module_import_reset(name); }
    [[nodiscard]] static constexpr auto const* find_import_rewrite(
        ::uwvm2::utils::container::u8string_view name,
        ::uwvm2::utils::container::u8string_view imported_module,
        ::uwvm2::utils::container::u8string_view imported_name) noexcept
    { return ::uwvm2::uwvm::wasm::storage::find_configured_import_reset_const(name, imported_module, imported_name); }
    [[nodiscard]] static constexpr auto const* find_memory_limit(::uwvm2::utils::container::u8string_view name) noexcept
    { return ::uwvm2::uwvm::wasm::storage::find_configured_module_memory_limit_const(name); }
    [[nodiscard]] ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store> make_store(
        ::uwvm2::uwvm::runtime::storage::gc_type::recursive_type_section const& section,
        ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_lease_owner> const& leases,
        ::uwvm2::uwvm::runtime::storage::gc_function_type_match_callback callback) const noexcept;
};
