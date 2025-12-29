/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/impl.h>
# include <uwvm2/object/impl.h>
# include <uwvm2/uwvm/wasm/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    /// @brief Function section storage

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_function_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...>{}; }

    using wasm_binfmt1_final_function_type_t = decltype(get_final_function_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_wasm_code_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_code_t<Fs...>{}; }

    using wasm_binfmt1_final_wasm_code_t = decltype(get_final_wasm_code_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

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

    struct imported_function_storage_t
    {
        // If unresolved, both pointers are null.
        // If resolved, exactly one of the pointers is non-null.
        imported_function_storage_t const* imported_ptr{};
        local_defined_function_storage_t const* defined_ptr{};
        wasm_binfmt1_final_import_type_t const* import_type_ptr{};

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
    /// @brief Table section storage

    enum class local_defined_table_elem_storage_type_t : unsigned
    {
        func_ref_imported,
        func_ref_defined,
        /// @todo Reference Types
    };

    struct local_defined_table_elem_storage_t
    {
        union imported_function_storage_u
        {
            // For other uses of WASM, the prerequisite is that WASM must be initialized.
            imported_function_storage_t const* imported_ptr;
            local_defined_function_storage_t const* defined_ptr;

            /// @todo Reference Types
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
}

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_table_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_table_type<Fs...>{}; }

    using wasm_binfmt1_final_table_type_t = decltype(get_final_table_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    struct local_defined_table_storage_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        ::uwvm2::utils::container::vector<local_defined_table_elem_storage_t> elems{};
        static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::utils::container::vector<local_defined_table_elem_storage_t>>);

        wasm_binfmt1_final_table_type_t const* table_type_ptr{};
    };

    struct imported_table_storage_t
    {
        // If unresolved, both pointers are null.
        // If resolved, exactly one of the pointers is non-null.
        imported_table_storage_t const* imported_ptr{};
        local_defined_table_storage_t const* defined_ptr{};
        wasm_binfmt1_final_import_type_t const* import_type_ptr{};

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
            ::fast_io::freestanding::is_zero_default_constructible_v<::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_table_type_t const*>;
    };

    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t>
    {
        inline static constexpr bool value =
            ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t>> &&
            ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_table_type_t const*>;
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

    struct local_defined_memory_storage_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        // NOTE: `native_memory_t` is a real runtime object; its default constructor may allocate (e.g. mmap backend).
        // If you need a "module record" that is cheap to construct (no allocations) before instantiation,
        // store only `memory_type_ptr` here and move the actual memory objects into a separate instance storage.
        ::uwvm2::object::memory::linear::native_memory_t memory{};

        wasm_binfmt1_final_memory_type_t const* memory_type_ptr{};
    };

    struct imported_memory_storage_t
    {
        // If unresolved, both pointers are null.
        // If resolved, exactly one of the pointers is non-null.
        imported_memory_storage_t const* imported_ptr{};
        local_defined_memory_storage_t const* defined_ptr{};
        wasm_binfmt1_final_import_type_t const* import_type_ptr{};

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
        // If unresolved, both pointers are null.
        // If resolved, exactly one of the pointers is non-null.
        imported_global_storage_t const* imported_ptr{};
        local_defined_global_storage_t const* defined_ptr{};
        wasm_binfmt1_final_import_type_t const* import_type_ptr{};

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

        // Here, `uint_fast8_t` is used to ensure alignment with `bool` for efficient access.
        wasm_element_segment_kind kind{wasm_element_segment_kind::active};

        // meaningful only for passive segments; when true the payload is not available.
        bool dropped{};
    };

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

        // meaningful only for passive segments; when true the payload is not available.
        // `data.drop` will set `byte_begin`/`byte_end` to nullptr, making it easier to validate.
        bool dropped{};
    };

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
        // func
        ::uwvm2::utils::container::vector<imported_function_storage_t> imported_function_vec_storage{};
        ::uwvm2::utils::container::vector<local_defined_function_storage_t> local_defined_function_vec_storage{};

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

        // code
        ::uwvm2::utils::container::vector<local_defined_code_storage_t> local_defined_code_vec_storage{};

        // data
        ::uwvm2::utils::container::vector<local_defined_data_storage_t> local_defined_data_vec_storage{};
    };
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::runtime::storage::wasm_module_storage_t>
    {
        inline static constexpr bool value = ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::imported_function_storage_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t>> &&
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
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_code_storage_t>> &&
                                             ::fast_io::freestanding::is_zero_default_constructible_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t>>;
    };

    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::uwvm::runtime::storage::wasm_module_storage_t>
    {
        inline static constexpr bool value = ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::imported_function_storage_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t>> &&
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
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_code_storage_t>> &&
                                             ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<
                                                 ::uwvm2::utils::container::vector<::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t>>;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
