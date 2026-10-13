/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <atomic>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <type_traits>
# include <limits>
# include <memory>
# include <uwvm2/runtime/gc/instance_phase.h>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/allocator/fast_io_strict/impl.h>
# include <uwvm2/parser/wasm/base/error_code.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/impl.h>
# include <uwvm2/validation/standard/wasm3/recursive_type_validation.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/impl.h>
# include <uwvm2/object/impl.h>
# include "tag_instance_identity.h"
# include <uwvm2/uwvm/wasm/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#include "gc_object.h"

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    /// @brief type section storage
    /// @brief Function section storage

    /// @warning Extension point: runtime final_* aliases must be audited whenever wasm_binfmt1_features gains a standard feature.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_function_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...>{}; }

    using wasm_binfmt1_final_function_type_t = decltype(get_final_function_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_value_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>{}; }

    using wasm_binfmt1_final_value_type_t = decltype(get_final_value_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));
    using wasm_binfmt1_owned_signature_t = ::uwvm2::parser::wasm::standard::wasm3::type::owned_function_signature<
        wasm_binfmt1_final_value_type_t>;

    struct type_section_storage_t
    {
        // Parser-owned subtype forest; borrowed for the module lifetime and consulted only during compilation.
        ::uwvm2::validation::standard::wasm3::recursive_type_context const* core3_context_ptr{};
        // Borrow this actual context object from the retained parser type section,
        // including its empty legacy 0x60 function-only model. The parser-module owner
        // must keep the object's address stable for the compilation borrow, matching
        // the existing signature and core3_context_ptr lifetime contract.
        ::uwvm2::validation::standard::wasm3::recursive_type_context const* core3_declaration_context_ptr{};
        // Parser-owned aggregate declarations are immutable for the entire runtime-module lifetime.
        // Compiler validation borrows field layouts here; execution uses gc_store's private copy.
        ::uwvm2::parser::wasm::standard::wasm3::type::recursive_type_section const* core3_recursive_types_ptr{};
        wasm_binfmt1_final_function_type_t const* type_section_begin{};
        wasm_binfmt1_final_function_type_t const* type_section_end{};
        // The parser's checked flat count also covers legacy 0x60 function-only
        // sections, which have no recursive context but can declare typed refs.
        ::std::size_t type_section_count{};
        // The parser owns the exact Core 3 heap/nullability declarations. These borrowed pointers
        // are null together when no rich signature array exists; otherwise they align with type indices.
        wasm_binfmt1_owned_signature_t const* owned_signature_begin{};
        wasm_binfmt1_owned_signature_t const* owned_signature_end{};
        // Declaration encoding policy survives projection to the runtime signature carriers.
        bool requires_function_references{};
        bool requires_gc{};
        bool requires_exceptions{};
        bool requires_simd{}, requires_reference_types{}, requires_multi_value{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_wasm_code_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_code_t<Fs...>{}; }

    using wasm_binfmt1_final_wasm_code_t = decltype(get_final_wasm_code_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_wasm_const_expr_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...>{}; }

    using wasm_binfmt1_final_wasm_const_expr_t = decltype(get_final_wasm_const_expr_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    // Tag identity survives any throwing activation and can be retained independently of parser
    // signatures/module storage by an exception value. Never substitute a structural function type:
    // two separately instantiated tags with equal signatures must remain distinguishable.
    struct local_defined_tag_storage_t
    {
        wasm_binfmt1_final_function_type_t const* function_type_ptr{};
        ::std::size_t type_index{};
        ::std::shared_ptr<tag_instance_identity const> exception_identity{};
    };

    struct local_defined_function_storage_t
    {
        // Parsed pointer via ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_t
        wasm_binfmt1_final_function_type_t const* function_type_ptr{};
        // Since each function corresponds to a specific code section, pointers are provided here.
        wasm_binfmt1_final_wasm_code_t const* wasm_code_ptr{};
        // No pointers to code storage are provided here. To prevent complications arising from broken bidirectional pointers and iterators, the code must be
        // fully constructed beforehand and remain unmodified.
    };
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t>);
}

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    struct wasm_module_storage_t;

    enum class wasm_global_init_state : unsigned
    {
        uninitialized,
        initializing,
        initialized
    };

    using local_defined_function_vec_storage_t = ::uwvm2::utils::container::vector<local_defined_function_storage_t>;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_import_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_import_type<Fs...>{}; }

    using wasm_binfmt1_final_import_type_t = decltype(get_final_import_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    struct imported_tag_storage_t
    {
        wasm_binfmt1_final_import_type_t const* import_type_ptr{};
        wasm_binfmt1_final_function_type_t const* function_type_ptr{};
        imported_tag_storage_t const* imported_target{};
        local_defined_tag_storage_t const* defined_target{};
        local_defined_tag_storage_t const* resolved_tag{};
    };

    /// @warning Extension point: new import backends require target union members plus initializer/linker/compiler dispatch updates.
    enum class imported_function_link_kind : unsigned
    {
        unresolved,
        imported,
        defined,
#if defined(UWVM_SUPPORT_PRELOAD_DL)
        dl,
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        weak_symbol,
#endif
        local_imported
    };

    struct imported_function_storage_t
    {
        struct local_imported_target_t
        {
            ::uwvm2::uwvm::wasm::type::local_imported_t* module_ptr{};
            ::std::size_t index{};
        };

        union imported_function_target_u
        {
            // If unresolved, this holds a null pointer.
            imported_function_storage_t const* imported_ptr;
            local_defined_function_storage_t const* defined_ptr;
#if defined(UWVM_SUPPORT_PRELOAD_DL)
            ::uwvm2::uwvm::wasm::type::capi_function_t const* dl_ptr;
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
            ::uwvm2::uwvm::wasm::type::capi_function_t const* weak_symbol_ptr;
#endif
            local_imported_target_t local_imported;
        };

        // If unresolved, `link_kind == unresolved` and `target.imported_ptr == nullptr`.
        // If resolved, the active `target` member is specified by `link_kind`.
        imported_function_target_u target{};
        wasm_binfmt1_final_import_type_t const* import_type_ptr{};
        imported_function_link_kind link_kind{imported_function_link_kind::unresolved};

        // Is the opposite side of this imported function also imported or custom?
        bool is_opposite_side_imported{};
    };

    using imported_function_vec_storage_t = ::uwvm2::utils::container::vector<imported_function_storage_t>;
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::imported_function_storage_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::imported_function_storage_t>);
}

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    // Runtime table-mutation bridges retain the concrete operation in their ABI while sharing one exact-range updater.
    // The resolved local-defined table pointer, rather than a caller's table index, identifies cross-module import aliases.
    enum class llvm_jit_call_indirect_table_mutation_kind : unsigned char
    {
        set,
        init,
        copy,
        fill,
        grow
    };

#if defined(UWVM_RUNTIME_LLVM_JIT)
    struct llvm_jit_raw_call_target_t
    {
        ::std::uintptr_t entry_address{};
        ::std::uintptr_t context_address{};
        // Caller-local Core 3 forest preorder, or legacy canonical signature
        // index. UINT32_MAX is outside every admitted callable type interval.
        ::std::uint_least32_t encoded_type_id{};
        ::std::uintptr_t typed_entry_address{};
    };

    struct llvm_jit_call_indirect_table_view_t
    {
        ::std::uintptr_t data_address{};
        ::std::size_t size{};
    };
#endif

    /// @brief Table section storage

    /// @warning Extension point: new table element payload kinds require parser element handling, initializer writes, and compiler table access updates.
    enum class local_defined_table_elem_storage_type_t : unsigned
    {
        func_ref_imported,
        func_ref_defined,
        /// @brief Host-owned opaque reference. A null pointer represents `ref.null extern`.
        /// @details Appending this enumerator preserves the numeric values and fast-path layout used by existing funcref tables.
        extern_ref,
        /// @brief A VM-owned Core 3 exception token; null is represented by a null payload.
        /// @details Keep the existing pointer plus kind table-slot ABI so function tables retain their compact fast path.
        exn_ref,
        /// @brief Core 3 GC values retain their kind in the existing 16-byte slot.
        gc_i31_ref,
        gc_struct_ref,
        gc_array_ref,
    };

    struct local_defined_table_elem_storage_t
    {
        union imported_function_storage_u
        {
            // For other uses of WASM, the prerequisite is that WASM must be initialized.
            imported_function_storage_t const* imported_ptr;
            local_defined_function_storage_t const* defined_ptr;
            void* extern_ptr;
            ::uwvm2::parser::wasm::standard::wasm3::type::wasm_i31 wasm_i31;
        };

        imported_function_storage_u storage{};

        local_defined_table_elem_storage_type_t type{};
    };
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t>);
    static_assert(sizeof(::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t) ==
                  sizeof(::uwvm2::uwvm::runtime::storage::gc_reference));
}

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_table_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_table_type<Fs...>{}; }

    using wasm_binfmt1_final_table_type_t = decltype(get_final_table_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    struct local_defined_table_storage_t
    {
        ::uwvm2::utils::container::vector<local_defined_table_elem_storage_t> elems{};
        static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::utils::container::vector<local_defined_table_elem_storage_t>>);

        wasm_binfmt1_final_table_type_t const* table_type_ptr{};
        // Borrowed from the parsed module, whose lifetime also owns table_type_ptr; null for implicit ref.null.
        wasm_binfmt1_final_wasm_const_expr_t const* initializer_expr{};
        wasm_module_storage_t* owner_module_rt_ptr{};
    };

#if defined(UWVM_RUNTIME_LLVM_JIT)
    // Cold notifications use the actual resolved table identity, including
    // every imported alias. A standalone parser/initializer/translator leaves
    // this hook null and does not acquire a runtime object link dependency.
    // Full/lazy LLVM publication installs it; drained reset detaches it before
    // destroying views. The existing runtime bridge serializes exact writes.
    using llvm_jit_table_refresh_hook_t = void (*)(local_defined_table_storage_t*,
        llvm_jit_call_indirect_table_mutation_kind, ::std::size_t, ::std::size_t) noexcept;
    inline llvm_jit_table_refresh_hook_t llvm_jit_table_refresh_hook{};
#endif

    /// @brief Grow a live table without turning host allocation failure into a Wasm trap.
    /// @note Tables are not shared by the threads proposal. The VM owns mutation and refreshes JIT views after success.
    [[nodiscard]] inline constexpr bool try_grow_table_elements(local_defined_table_storage_t& table,
        ::std::size_t new_size, local_defined_table_elem_storage_t fill) noexcept
    {
        using element = local_defined_table_elem_storage_t;
        using strict = ::uwvm2::utils::allocator::fast_io_strict::native_strict_typed_global_allocator<element>;
        static_assert(::std::is_trivially_copyable_v<element> && ::std::is_trivially_destructible_v<element>);
        auto& elements{table.elems};
        auto const old_size{elements.size()};
        constexpr auto host_limit{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(element)};
        if(new_size < old_size || new_size > host_limit) [[unlikely]] { return false; }
        if(new_size == old_size) { return true; }
        if consteval
        {
            elements.resize(new_size);
            for(auto index{old_size}; index != new_size; ++index) { elements.index_unchecked(index) = fill; }
            return true;
        }
        else
        {
            // fast_io exposes its vector model; keep its allocator family, alignment and three-pointer ownership unchanged.
            // [old_begin ... old_size ... capacity) is the allocation retained by this vector.
            // [safe                              ] null denotes the empty vector, never a range to subtract.
            auto const old_begin{elements.imp.begin_ptr};
            auto const capacity{old_begin == nullptr ? 0uz : static_cast<::std::size_t>(elements.imp.end_ptr - old_begin)};
            auto new_begin{old_begin};
            if(new_size > capacity)
            {
                // [old allocation] remains live if allocation/reallocation returns null.
                // [safe          ] new_size * sizeof(element) is representable and <= PTRDIFF_MAX.
                if constexpr(strict::has_reallocate) { new_begin = strict::reallocate(old_begin, new_size); }
                else
                {
                    new_begin = strict::allocate(new_size);
                    if(new_begin == nullptr) [[unlikely]] { return false; }
                    if(old_size != 0uz) { ::std::memcpy(new_begin, old_begin, old_size * sizeof(element)); }
                    strict::deallocate_n(old_begin, capacity);
                    // old_begin is invalid after deallocation and is never read again.
                }
                if(new_begin == nullptr) [[unlikely]] { return false; }
                // [new_begin ... new_size) is a complete new allocation preserving the old live slots.
                // [safe                 ] publish the owner and its one-past capacity together after success.
                elements.imp.begin_ptr = new_begin;
                elements.imp.end_ptr = new_begin + new_size;
            }
            // [old_size ... new_size) fits the proven capacity; construct only new trivially copyable slots.
            for(auto index{old_size}; index != new_size; ++index)
            {
                // [new_begin ... index ... new_size) <- index is strictly below the allocation end.
                // [safe                           ] implicit-lifetime slots may receive the complete reference value.
                new_begin[index] = fill;
            }
            // [new_begin ... new_size] the initialized prefix is now complete; one-past is stored, never dereferenced.
            // [safe                 ]
            elements.imp.curr_ptr = new_begin + new_size;
            return true;
        }
    }

    struct imported_table_storage_t
    {
        enum class imported_table_link_kind : unsigned
        {
            unresolved = 0u,
            imported,
            defined
        };

        union imported_table_target_u
        {
            imported_table_storage_t const* imported_ptr;
            local_defined_table_storage_t* defined_ptr;
        };

        // If unresolved, `link_kind == unresolved` and `target.imported_ptr == nullptr`.
        // If resolved, the active `target` member is specified by `link_kind`.
        imported_table_target_u target{};
        wasm_binfmt1_final_import_type_t const* import_type_ptr{};
        imported_table_link_kind link_kind{imported_table_link_kind::unresolved};

        // Is the opposite side of this imported table also imported or custom?
        bool is_opposite_side_imported{};
    };
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t>
    {
        inline static constexpr bool value =
            ::fast_io::freestanding::is_zero_default_constructible_v<
                ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t>> &&
            ::fast_io::freestanding::is_zero_default_constructible_v<::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_table_type_t const*> &&
            ::fast_io::freestanding::is_zero_default_constructible_v<::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const*> &&
            ::fast_io::freestanding::is_zero_default_constructible_v<::uwvm2::uwvm::runtime::storage::wasm_module_storage_t*>;
    };

    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t>
    {
        inline static constexpr bool value =
            ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t>> &&
            ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_table_type_t const*> &&
            ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const*> &&
            ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::wasm_module_storage_t*>;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::imported_table_storage_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t>);
    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::imported_table_storage_t>);
}

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    /// @brief Memory section storage

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_memory_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_memory_type<Fs...>{}; }

    using wasm_binfmt1_final_memory_type_t = decltype(get_final_memory_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    struct checkpoint_effective_memory_bounds_t
    {
        ::std::uint64_t minimum{}, maximum{};
    };
    // Cold checkpoint DATA projection only. The caller already authenticated the
    // actual record/source/configuration under its real native management proof.
    // Ordinary overrides REPLACE declared limits (including warned widening), so
    // never intersect the resulting policy with the original declaration again.
    // The Wasm address-domain ceiling is independent of native allocation quota.
    [[nodiscard]] inline constexpr checkpoint_effective_memory_bounds_t checkpoint_effective_memory_bounds(
        bool memory64, ::uwvm2::uwvm::wasm::type::module_memory_limit_t const& effective) noexcept
    {
        ::std::uint64_t const address_max{memory64 ? (::std::uint64_t{1u} << 48u) : 65536u};
        auto const native_max{static_cast<::std::uint64_t>(effective.max)};
        return {static_cast<::std::uint64_t>(effective.min),
            effective.present_max && native_max < address_max ? native_max : address_max};
    }

    /// @warning Extension point: memory64/multi-memory changes must audit effective limits, pointer sizes, allocator setup, and compiler memory operands.
    struct local_defined_memory_storage_t
    {
        // NOTE: `native_memory_t` is a real runtime object; its default constructor may allocate (e.g. mmap backend).
        // If you need a "module record" that is cheap to construct (no allocations) before instantiation,
        // store only `memory_type_ptr` here and move the actual memory objects into a separate instance storage.
        ::uwvm2::object::memory::linear::native_memory_t memory{};

        wasm_binfmt1_final_memory_type_t const* memory_type_ptr{};
        ::uwvm2::uwvm::wasm::type::module_memory_limit_t effective_limits{};
    };

    struct imported_memory_storage_t
    {
        enum class imported_memory_link_kind : unsigned
        {
            unresolved = 0u,
            imported,
            defined,
            local_imported
        };

        struct local_imported_target_t
        {
            ::uwvm2::uwvm::wasm::type::local_imported_t* module_ptr{};
            ::std::size_t index{};
        };

        union imported_memory_target_u
        {
            imported_memory_storage_t const* imported_ptr;
            local_defined_memory_storage_t* defined_ptr;
            local_imported_target_t local_imported;
            static_assert(::std::is_trivially_copyable_v<local_imported_target_t> && ::std::is_trivially_destructible_v<local_imported_target_t>);
        };

        // If unresolved, `link_kind == unresolved` and `target.imported_ptr == nullptr`.
        // If resolved, the active `target` member is specified by `link_kind`.
        imported_memory_target_u target{};
        wasm_binfmt1_final_import_type_t const* import_type_ptr{};
        ::uwvm2::uwvm::wasm::type::module_memory_limit_t effective_limits{};
        imported_memory_link_kind link_kind{imported_memory_link_kind::unresolved};

        // Is the opposite side of this imported memory also imported or custom?
        bool is_opposite_side_imported{};
    };
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t>
    {
        // `native_memory_t` has a non-trivial default constructor (it may allocate).
        // We only declare trivial relocatability when the selected backend is explicitly marked relocatable.
        inline static constexpr bool value = ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::object::memory::linear::native_memory_t>;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::imported_memory_storage_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::imported_memory_storage_t>);
}

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    /// @brief Global section storage

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_global_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_global_type<Fs...>{}; }

    using wasm_binfmt1_final_global_type_t = decltype(get_final_global_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_local_global_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_local_global_type<Fs...>{}; }

    using wasm_binfmt1_final_local_global_type_t = decltype(get_final_local_global_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    /// @warning Extension point: new global payload categories must audit initializer const_expr evaluation and global.get/global.set runtime access.
    struct local_defined_global_storage_t
    {
        ::uwvm2::object::global::wasm_global_storage_t global{};

        wasm_binfmt1_final_global_type_t const* global_type_ptr{};
        wasm_binfmt1_final_local_global_type_t const* local_global_type_ptr{};

        // These two fields are only meaningful during/after instantiation initialization.
        // They enable correct evaluation of wasm1 global initializers that use `global.get`.
        wasm_module_storage_t* owner_module_rt_ptr{};
        wasm_global_init_state init_state{wasm_global_init_state::uninitialized};
    };

    struct imported_global_storage_t
    {
        enum class imported_global_link_kind : unsigned
        {
            unresolved = 0u,
            imported,
            defined,
            local_imported
        };

        struct local_imported_target_t
        {
            ::uwvm2::uwvm::wasm::type::local_imported_t* module_ptr{};
            ::std::size_t index{};
        };

        union imported_global_target_u
        {
            imported_global_storage_t const* imported_ptr;
            local_defined_global_storage_t* defined_ptr;
            local_imported_target_t local_imported;
        };

        // If unresolved, `link_kind == unresolved` and `target.imported_ptr == nullptr`.
        // If resolved, the active `target` member is specified by `link_kind`.
        imported_global_target_u target{};
        wasm_binfmt1_final_import_type_t const* import_type_ptr{};
        imported_global_link_kind link_kind{imported_global_link_kind::unresolved};

        // Is the opposite side of this imported global also imported or custom?
        bool is_opposite_side_imported{};
    };
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t>);

    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::imported_global_storage_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::imported_global_storage_t>);
}

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    /// @brief Element section storage

    // Here, `uint_fast8_t` is used to ensure alignment with `bool` for efficient access.
    /// @warning Extension point: new element segment runtime states must be synchronized with parser flags and instantiation/drop operations.
    enum class wasm_element_segment_kind : ::std::uint_fast8_t
    {
        /// @brief Active segment: applied during instantiation (elem section in wasm1 MVP).
        active,
        /// @brief Passive segment: retained for runtime `table.init` / `elem.drop` (bulk memory + reference types).
        passive,
    };

    struct wasm_element_storage_t
    {
        using func_idx_t = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;

        // table index to initialize
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_idx{};

        // element offset into the table (valid only for active segments)
        ::std::uint_least64_t offset{};

        // initializer function indices (module function index space)
        func_idx_t const* funcidx_begin{};
        func_idx_t const* funcidx_end{};

        // Runtime-owned canonical funcrefs for expression-form segments. Unlike a bare function index, these pointer-based entries
        // preserve the creating module when an imported global supplies a reference from another module.
        local_defined_table_elem_storage_t const* funcref_begin{};
        local_defined_table_elem_storage_t const* funcref_end{};

        // Opaque host references for externref expression segments. Null entries are `ref.null extern`.
        void* const* externref_begin{};
        void* const* externref_end{};

        // Core 3 expression segments carry the complete reference, including
        // i31 bits and struct/array kind. Legacy pointer-only segments stay compact.
        gc_reference const* gc_ref_begin{};
        gc_reference const* gc_ref_end{};

        // Here, `uint_fast8_t` is used to ensure alignment with `bool` for efficient access.
        wasm_element_segment_kind kind{wasm_element_segment_kind::active};

        // When true the payload is unavailable. Active segments are dropped after successful instantiation;
        // passive/declarative segments are dropped by elem.drop or while being instantiated.
        // Host publication freezes all payload pointers. Concurrent drop
        // changes only this flag; module teardown first drains execution leases.
        alignas(::std::atomic_ref<bool>::required_alignment) bool dropped{};
    };

    struct wasm_element_payload_t
    {
        wasm_element_storage_t::func_idx_t const* funcidx_begin{};
        wasm_element_storage_t::func_idx_t const* funcidx_end{};
        local_defined_table_elem_storage_t const* funcref_begin{};
        local_defined_table_elem_storage_t const* funcref_end{};
        void* const* externref_begin{};
        void* const* externref_end{};
        gc_reference const* gc_ref_begin{};
        gc_reference const* gc_ref_end{};
    };

    [[nodiscard]] inline constexpr bool wasm_element_segment_is_dropped(wasm_element_storage_t const& element) noexcept
    {
        if consteval { return element.dropped; }
        else
        {
            // Some supported libraries lack atomic_ref<const T>. This alias is
            // used only for an atomic load, including when the instance is const.
            return ::std::atomic_ref<bool>{const_cast<bool&>(element.dropped)}.load(::std::memory_order_acquire);
        }
    }

    [[nodiscard]] inline constexpr wasm_element_payload_t load_wasm_element_segment_payload(wasm_element_storage_t const& element) noexcept
    {
        if(wasm_element_segment_is_dropped(element)) { return {}; }
        // [module-owned immutable payload ...] end
        // [safe                              ]
        // ^^ each returned pair borrows its original allocation, published before
        // guest entry. A concurrent drop cannot invalidate this acquired view;
        // the caller's execution lease retains both instance and reference owners.
        return {element.funcidx_begin, element.funcidx_end, element.funcref_begin, element.funcref_end,
                element.externref_begin, element.externref_end, element.gc_ref_begin, element.gc_ref_end};
    }

    /// @brief Atomically make an element instance logically empty, retaining acquired snapshots.
    inline constexpr void drop_wasm_element_segment_payload(wasm_element_storage_t& element) noexcept
    {
        if consteval { element.dropped = true; }
        else { ::std::atomic_ref<bool>{element.dropped}.store(true, ::std::memory_order_release); }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_element_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_element_type_t<Fs...>{}; }

    using wasm_binfmt1_final_element_type_t = decltype(get_final_element_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    // Keep a pointer to the parser's element record for now (not fully decayed).
    struct local_defined_element_storage_t
    {
        wasm_element_storage_t element{};

        wasm_binfmt1_final_element_type_t const* element_type_ptr{};
    };
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::wasm_element_storage_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::wasm_element_storage_t>);

    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::local_defined_element_storage_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::local_defined_element_storage_t>);
}

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    /// @brief Code section storage
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_code_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_code_t<Fs...>{}; }

    using wasm_binfmt1_final_code_type_t = decltype(get_final_code_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    struct local_defined_code_storage_t
    {
        wasm_binfmt1_final_code_type_t const* code_type_ptr{};
        local_defined_function_storage_t const* func_ptr{};

        /// @todo non-image compiler
    };
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::local_defined_code_storage_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::local_defined_code_storage_t>);
}

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    /// @brief Data section storage

    // Here, `uint_fast8_t` is used to ensure alignment with `bool` for efficient access.
    enum class wasm_data_segment_kind : ::std::uint_fast8_t
    {
        /// @brief Active segment: applied during instantiation (data section in wasm1 MVP).
        active,
        /// @brief Passive segment: retained for runtime `memory.init` / `data.drop` (bulk memory feature).
        passive,
    };

    struct wasm_data_storage_t
    {
        using byte_type = ::std::byte;

        byte_type const* byte_begin{};
        byte_type const* byte_end{};

        // target memory index
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memory_idx{};

        // byte offset into target memory (valid only for active segments)
        ::std::uint_least64_t offset{};

        // Here, `uint_fast8_t` is used to ensure alignment with `bool` for efficient access.
        wasm_data_segment_kind kind{wasm_data_segment_kind::active};

        // When true the payload is unavailable. Active segments are dropped after successful instantiation;
        // `data.drop` performs the same transition for passive segments.
        // Initialization precedes host-entry publication. Once live, every
        // access to this flag goes through atomic_ref; payload pointers remain
        // immutable until execution leases have drained at module teardown.
        alignas(::std::atomic_ref<bool>::required_alignment) bool dropped{};
    };

    struct wasm_data_payload_t
    {
        ::std::byte const* byte_begin{};
        ::std::byte const* byte_end{};
    };

    [[nodiscard]] inline constexpr bool wasm_data_segment_is_dropped(wasm_data_storage_t const& data) noexcept
    {
        if consteval { return data.dropped; }
        else
        {
            // atomic_ref<const T> is not implemented by every supported library.
            // This alias performs only an atomic load; it never writes through
            // a const object. The declared flag remains trivially relocatable.
            return ::std::atomic_ref<bool>{const_cast<bool&>(data.dropped)}.load(::std::memory_order_acquire);
        }
    }

    [[nodiscard]] inline constexpr wasm_data_payload_t load_wasm_data_segment_payload(wasm_data_storage_t const& data) noexcept
    {
        if(wasm_data_segment_is_dropped(data)) { return {}; }
        // [module-owned immutable bytes ...] end; both pointers were published
        // [safe                           ] together before any guest entry.
        // ^^ returned begin/end borrow the same allocation. A concurrent drop
        // changes only the flag and cannot invalidate this acquired snapshot.
        // Module teardown waits for the caller's execution lease to end.
        return {data.byte_begin, data.byte_end};
    }

    /// @brief Make a data instance logically empty without mutating live pointer pairs.
    inline constexpr void drop_wasm_data_segment_payload(wasm_data_storage_t& data) noexcept
    {
        if consteval { data.dropped = true; }
        else { ::std::atomic_ref<bool>{data.dropped}.store(true, ::std::memory_order_release); }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_data_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_data_type_t<Fs...>{}; }

    using wasm_binfmt1_final_data_type_t = decltype(get_final_data_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    struct local_defined_data_storage_t
    {
        wasm_data_storage_t data{};

        wasm_binfmt1_final_data_type_t const* data_type_ptr{};
    };
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::wasm_data_storage_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::wasm_data_storage_t>);

    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t>);
}

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    struct wasm_module_storage_t
    {
        // Native initializer and actual segment installation publish this cold
        // state. It is never read by ordinary memory/field guest instructions.
        ::uwvm2::runtime::gc::instance_collection_phase gc_collection_phase{};
#if defined(UWVM_RUNTIME_LLVM_JIT)
        // Borrowed from the global module-name table.  LLVM JIT uses this stable identity for IR symbol names so
        // cache keys do not depend on runtime storage addresses.
        ::uwvm2::utils::container::u8string_view module_name{};
#endif

        // type
        // Exposes the binfmt1 type section for compiler-side validation (e.g. call_indirect: type_index -> signature).
        type_section_storage_t type_section_storage{};
        // One immutable type layout and object arena per runtime module instance.
        // Initialized before guest publication; shared ownership preserves object lifetime
        // across map moves and references held by a linked importing module.
        ::std::shared_ptr<gc_object_store> gc_store{};
        // Independent module-owned roots for GC stores borrowed through wrapped
        // externrefs. They are released at this module's reset, even when another
        // module still keeps this module's gc_store alive.
        ::std::shared_ptr<gc_lease_owner> gc_lease_roots{};
        // Host-only declaration policy metadata; never consulted by generated guest instructions.
        bool table_declarations_require_function_references{};
        bool global_declarations_require_function_references{};
        bool element_declarations_require_function_references{};
        bool table_declarations_require_gc{};
        bool global_declarations_require_gc{};
        bool element_declarations_require_gc{};
        bool table_declarations_require_exceptions{};
        bool global_declarations_require_exceptions{};
        bool element_declarations_require_exceptions{};
        bool table_declarations_require_reference_types{}, global_declarations_require_reference_types{},
            element_declarations_require_reference_types{}, global_declarations_require_simd{};
        unsigned table_reference_types_diagnostic_value{}, global_reference_types_diagnostic_value{},
            element_reference_types_diagnostic_value{};
        bool memory_declarations_require_memory64{}, table_declarations_require_table64{},
            memory_declarations_require_threads{}, memory_declarations_require_multi_memory{};
        bool table_declarations_require_table_initializer{}, constant_expressions_require_extended_const{};
        unsigned extended_const_diagnostic_value{};
        ::uwvm2::parser::wasm::base::wasm1p1_error_subject extended_const_diagnostic_subject{};
        ::uwvm2::parser::wasm::base::constant_expression_opcode_requirements constant_expression_opcode_requirements{};
        bool code_declarations_require_exceptions{};
        bool tag_section_present{};
        ::uwvm2::utils::container::vector<imported_tag_storage_t> imported_tag_vec_storage{};
        // Each entry has a distinct instance identity, even when signatures match. Freeze before guest publication.
        ::uwvm2::utils::container::vector<local_defined_tag_storage_t> local_defined_tag_vec_storage{};

        // func
        ::uwvm2::utils::container::vector<imported_function_storage_t> imported_function_vec_storage{};
        ::uwvm2::utils::container::vector<local_defined_function_storage_t> local_defined_function_vec_storage{};

        // Runtime-owned import descriptors for command-line import binding rewrites.
        // Parser-owned descriptors are never mutated.
        ::uwvm2::utils::container::vector<wasm_binfmt1_final_import_type_t> rewritten_import_vec_storage{};

        // table
        ::uwvm2::utils::container::vector<imported_table_storage_t> imported_table_vec_storage{};
        ::uwvm2::utils::container::vector<local_defined_table_storage_t> local_defined_table_vec_storage{};

        // memory
        ::uwvm2::utils::container::vector<imported_memory_storage_t> imported_memory_vec_storage{};
        ::uwvm2::utils::container::vector<local_defined_memory_storage_t> local_defined_memory_vec_storage{};

        // global
        ::uwvm2::utils::container::vector<imported_global_storage_t> imported_global_vec_storage{};
        ::uwvm2::utils::container::vector<local_defined_global_storage_t> local_defined_global_vec_storage{};

        // element
        ::uwvm2::utils::container::vector<local_defined_element_storage_t> local_defined_element_vec_storage{};
        ::uwvm2::utils::container::vector<local_defined_table_elem_storage_t> element_expr_funcref_vec_storage{};
        ::uwvm2::utils::container::vector<void*> element_expr_externref_vec_storage{};
        ::uwvm2::utils::container::vector<gc_reference> element_expr_gc_ref_vec_storage{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> declared_ref_funcidx_vec_storage{};

        // code
        ::uwvm2::utils::container::vector<local_defined_code_storage_t> local_defined_code_vec_storage{};

        // data
        ::uwvm2::utils::container::vector<local_defined_data_storage_t> local_defined_data_vec_storage{};
        // Preserve section presence separately from its count: absent and present-with-zero are semantically distinct
        // for bulk-memory instruction validation.
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 data_count_section_count{};
        bool data_count_section_present{};

        // LLVM JIT call_indirect uses a compact runtime table-view side structure.
#if defined(UWVM_RUNTIME_LLVM_JIT)
        ::uwvm2::utils::container::vector<llvm_jit_call_indirect_table_view_t> llvm_jit_call_indirect_table_views{};
#endif
    };
    enum class runtime_table_reference_family : unsigned char { function, external, exception, gc };
    [[nodiscard]] inline runtime_table_reference_family runtime_core_reference_family(
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type const& type,
        type_section_storage_t const& types) noexcept
    {
        namespace core = ::uwvm2::parser::wasm::standard::wasm3::type;
        if(type.kind != core::value_kind::reference) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const heap{type.heap};
        if(heap.is_defined())
        {
            auto const index{static_cast<::std::uint_least64_t>(heap.code)};
            auto const* const context{types.core3_context_ptr};
            if(context != nullptr)
            {
                if(!context->contains(index)) [[unlikely]] { ::fast_io::fast_terminate(); }
                // [0, context.records.size()) contains(index) proved the entry live.
                auto const kind{context->records.index_unchecked(static_cast<::std::size_t>(index)).kind};
                return kind == core::composite_kind::function ? runtime_table_reference_family::function :
                       runtime_table_reference_family::gc;
            }
            // Legacy 0x60 sections contain only function definitions. Their
            // typed table/element references still name a flat type index,
            // although the parser has no recursive Core 3 context to borrow.
            // [0, type_section_count) bounds that index without subtracting
            // two borrowed pointers or dereferencing an absent context.
            if(types.requires_gc || index >= types.type_section_count) [[unlikely]]
            { ::fast_io::fast_terminate(); }
            return runtime_table_reference_family::function;
        }
        using abstract = core::abstract_heap_type;
        switch(static_cast<abstract>(heap.code))
        {
            case abstract::func: case abstract::nofunc: return runtime_table_reference_family::function;
            case abstract::extern_: case abstract::noextern: return runtime_table_reference_family::external;
            case abstract::exn: case abstract::noexn: return runtime_table_reference_family::exception;
            case abstract::any: case abstract::eq: case abstract::i31: case abstract::struct_:
            case abstract::array: case abstract::none: return runtime_table_reference_family::gc;
        }
        ::fast_io::fast_terminate();
    }
    [[nodiscard]] inline runtime_table_reference_family runtime_table_family(
        local_defined_table_storage_t const& table) noexcept
    {
        if(table.table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const& declaration{*table.table_type_ptr};
        if(!declaration.has_core_type)
        {
            switch(static_cast<unsigned>(declaration.reftype))
            {
                case 0x70u: return runtime_table_reference_family::function;
                case 0x6fu: return runtime_table_reference_family::external;
                case 0x69u: return runtime_table_reference_family::exception;
                default: ::fast_io::fast_terminate();
            }
        }
        if(table.owner_module_rt_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        return runtime_core_reference_family(declaration.core_type,
            table.owner_module_rt_ptr->type_section_storage);
    }
    [[nodiscard]] inline runtime_table_reference_family runtime_element_family(
        local_defined_element_storage_t const& element, wasm_module_storage_t const& owner) noexcept
    {
        if(element.element_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const& declaration{element.element_type_ptr->storage.segment};
        if(!declaration.has_core_type)
        {
            switch(static_cast<unsigned>(declaration.reftype))
            {
                case 0x70u: return runtime_table_reference_family::function;
                case 0x6fu: return runtime_table_reference_family::external;
                case 0x69u: return runtime_table_reference_family::exception;
                default: ::fast_io::fast_terminate();
            }
        }
        return runtime_core_reference_family(declaration.core_type, owner.type_section_storage);
    }
    [[nodiscard]] inline gc_object_status retain_runtime_table_reference(
        local_defined_table_storage_t const* table, gc_reference const& reference) noexcept
    {
        if(table == nullptr || table->owner_module_rt_ptr == nullptr)
        { return gc_object_status::invalid_store; }
        auto const* store{table->owner_module_rt_ptr->gc_store.get()};
        return uwvm2_gc_retain_reference(store, ::std::addressof(reference));
    }
    [[nodiscard]] inline constexpr local_defined_table_elem_storage_t runtime_table_slot_from_gc_reference(
        gc_reference const& reference) noexcept
    {
        using kind = ::uwvm2::object::global::wasm_ref_kind;
        using slot_kind = local_defined_table_elem_storage_type_t;
        local_defined_table_elem_storage_t slot{};
        switch(reference.kind)
        {
            case kind::wasm_null: return slot;
            case kind::wasm_func_imported:
                slot.storage.imported_ptr = static_cast<imported_function_storage_t const*>(reference.storage.ptr);
                slot.type = slot_kind::func_ref_imported;
                return slot;
            case kind::wasm_func_defined:
                slot.storage.defined_ptr = static_cast<local_defined_function_storage_t const*>(reference.storage.ptr);
                slot.type = slot_kind::func_ref_defined;
                return slot;
            case kind::wasm_extern:
                slot.storage.extern_ptr = reference.storage.ptr;
                slot.type = slot_kind::extern_ref;
                return slot;
            case kind::wasm_exn:
                slot.storage.extern_ptr = reference.storage.ptr;
                slot.type = slot_kind::exn_ref;
                return slot;
            case kind::wasm_i31:
                slot.storage.wasm_i31 = reference.storage.wasm_i31;
                slot.type = slot_kind::gc_i31_ref;
                return slot;
            case kind::wasm_struct:
                slot.storage.extern_ptr = reference.storage.ptr;
                slot.type = slot_kind::gc_struct_ref;
                return slot;
            case kind::wasm_array:
                slot.storage.extern_ptr = reference.storage.ptr;
                slot.type = slot_kind::gc_array_ref;
                return slot;
            case kind::wasm_func: break; // A bare index must be canonicalized by its owning module first.
        }
        ::fast_io::fast_terminate();
    }
    [[nodiscard]] inline constexpr gc_reference runtime_table_slot_to_gc_reference(
        local_defined_table_elem_storage_t const& slot) noexcept
    {
        using kind = ::uwvm2::object::global::wasm_ref_kind;
        using slot_kind = local_defined_table_elem_storage_type_t;
        gc_reference reference{};
        switch(slot.type)
        {
            case slot_kind::func_ref_imported:
                reference.storage.ptr = const_cast<imported_function_storage_t*>(slot.storage.imported_ptr);
                reference.kind = reference.storage.ptr == nullptr ? kind::wasm_null : kind::wasm_func_imported;
                return reference;
            case slot_kind::func_ref_defined:
                reference.storage.ptr = const_cast<local_defined_function_storage_t*>(slot.storage.defined_ptr);
                reference.kind = reference.storage.ptr == nullptr ? kind::wasm_null : kind::wasm_func_defined;
                return reference;
            case slot_kind::extern_ref:
                reference.storage.ptr = slot.storage.extern_ptr;
                reference.kind = reference.storage.ptr == nullptr ? kind::wasm_null : kind::wasm_extern;
                return reference;
            case slot_kind::exn_ref:
                reference.storage.ptr = slot.storage.extern_ptr;
                reference.kind = reference.storage.ptr == nullptr ? kind::wasm_null : kind::wasm_exn;
                return reference;
            case slot_kind::gc_i31_ref:
                reference.storage.wasm_i31 = slot.storage.wasm_i31;
                reference.kind = kind::wasm_i31;
                return reference;
            case slot_kind::gc_struct_ref:
                reference.storage.ptr = slot.storage.extern_ptr;
                reference.kind = kind::wasm_struct;
                return reference;
            case slot_kind::gc_array_ref:
                reference.storage.ptr = slot.storage.extern_ptr;
                reference.kind = kind::wasm_array;
                return reference;
        }
        ::fast_io::fast_terminate();
    }
    [[nodiscard]] inline gc_object_status retain_runtime_table_extern_payload(
        local_defined_table_storage_t const* table, void* payload) noexcept
    {
        gc_reference reference{};
        reference.storage.ptr = payload;
        reference.kind = payload == nullptr ? ::uwvm2::object::global::wasm_ref_kind::wasm_null :
            ::uwvm2::object::global::wasm_ref_kind::wasm_extern;
        return retain_runtime_table_reference(table, reference);
    }
    [[nodiscard]] inline gc_object_status retain_runtime_table_exn_payload(
        local_defined_table_storage_t const* table, void* payload) noexcept
    {
        // The table slot contains only the opaque token and its exn_ref discriminant.
        // Reconstitute the complete reference carrier before validating/retaining it
        // in the table owner's store; a foreign module may retire independently.
        gc_reference reference{};
        reference.storage.ptr = payload;
        reference.kind = payload == nullptr ? ::uwvm2::object::global::wasm_ref_kind::wasm_null :
            ::uwvm2::object::global::wasm_ref_kind::wasm_exn;
        return retain_runtime_table_reference(table, reference);
    }
    [[nodiscard]] inline constexpr bool runtime_memory_is_address64(
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module, ::std::size_t index) noexcept
    {
        auto const declared_address64{[](auto const& memory) constexpr noexcept
        {
            if constexpr(requires { memory.address64; }) { return memory.address64; }
            else { return false; }
        }};
        auto const import_count{module.imported_memory_vec_storage.size()};
        if(index < import_count)
        {
            // The checked import index selects a live record. Its parsed declaration
            // remains owned by the module throughout initialization and execution.
            auto const declaration{module.imported_memory_vec_storage.index_unchecked(index).import_type_ptr};
            if(declaration == nullptr || declaration->imports.type !=
               ::uwvm2::parser::wasm::standard::wasm1::type::external_types::memory) [[unlikely]]
            { ::fast_io::fast_terminate(); }
            return declared_address64(declaration->imports.storage.memory);
        }
        auto const local_index{index - import_count};
        if(local_index >= module.local_defined_memory_vec_storage.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
        // Subtraction and the local bound prove that this declaration belongs to memory[index].
        auto const declaration{module.local_defined_memory_vec_storage.index_unchecked(local_index).memory_type_ptr};
        if(declaration == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        return declared_address64(*declaration);
    }
    [[nodiscard]] inline constexpr bool runtime_table_is_address64(
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module, ::std::size_t index) noexcept
    {
        auto const declared_address64{[](auto const& table) constexpr noexcept
        {
            if constexpr(requires { table.address64; }) { return table.address64; }
            else { return false; }
        }};
        auto const import_count{module.imported_table_vec_storage.size()};
        if(index < import_count)
        {
            // The checked import index selects a live record. Its parsed declaration
            // remains owned by the module throughout initialization and execution.
            auto const declaration{module.imported_table_vec_storage.index_unchecked(index).import_type_ptr};
            if(declaration == nullptr || declaration->imports.type !=
               ::uwvm2::parser::wasm::standard::wasm1::type::external_types::table) [[unlikely]]
            { ::fast_io::fast_terminate(); }
            return declared_address64(declaration->imports.storage.table);
        }
        auto const local_index{index - import_count};
        if(local_index >= module.local_defined_table_vec_storage.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
        // Subtraction and the local bound prove that this declaration belongs to table[index].
        auto const declaration{module.local_defined_table_vec_storage.index_unchecked(local_index).table_type_ptr};
        if(declaration == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        return declared_address64(*declaration);
    }


}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::wasm_module_storage_t>
    {
        inline static constexpr bool value = ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_lease_owner>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::imported_function_storage_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::imported_table_storage_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::imported_memory_storage_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::imported_global_storage_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_element_storage_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<void*>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_code_storage_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t>>
#if defined(UWVM_RUNTIME_LLVM_JIT)
                                             && ::fast_io::freestanding::is_zero_default_constructible_v<
                                                    ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_view_t>>
#endif
            ;
    };

    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::uwvm::runtime::storage::wasm_module_storage_t>
    {
        inline static constexpr bool value = ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_lease_owner>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::imported_function_storage_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::imported_table_storage_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::imported_memory_storage_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::imported_global_storage_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_element_storage_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<void*>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_code_storage_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t>>
#if defined(UWVM_RUNTIME_LLVM_JIT)
                                             && ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                    ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_view_t>>
#endif
            ;
    };
}

#include "gc_segment.h"

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
