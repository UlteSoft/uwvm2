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

    using local_defined_function_vec_storage_t = ::uwvm2::utils::container::vector<local_defined_function_storage_t>;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_import_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_import_type<Fs...>{}; }

    using wasm_binfmt1_final_import_type_t = decltype(get_final_import_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    struct imported_function_storage_t
    {
        // The imported function may have been imported from another module by the other party, or it may have been defined by the other party.
        union imported_function_storage_u
        {
            // For other uses of WASM, the prerequisite is that WASM must be initialized.
            imported_function_storage_t const* imported_ptr;
            local_defined_function_storage_t const* defined_ptr;
        };

        imported_function_storage_u storage{};
        wasm_binfmt1_final_import_type_t const* import_type_ptr{};

        // Is the opposite side of this imported logo also imported or custom?
        bool is_opposite_side_imported{};
    };

    using imported_function_vec_storage_t = ::uwvm2::utils::container::vector<imported_function_storage_t>;

    struct local_defined_table_elem_storage_t
    {
        union imported_function_storage_u
        {
            // For other uses of WASM, the prerequisite is that WASM must be initialized.
            imported_function_storage_t const* imported_ptr;
            local_defined_function_storage_t const* defined_ptr;
        };

        imported_function_storage_u storage{};

        bool is_imported{};
    };

    struct local_defined_table_storage_t
    {
        ::uwvm2::utils::container::vector<local_defined_table_elem_storage_t> elems{};
    };

    struct wasm_module_storage_t
    {
        // func
        ::uwvm2::utils::container::vector<imported_function_storage_t> imported_function_vec_storage{};
        ::uwvm2::utils::container::vector<local_defined_function_storage_t> local_defined_function_vec_storage{};

        // table
        ::uwvm2::utils::container::vector<local_defined_table_storage_t> local_defined_table_vec_storage{};
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
