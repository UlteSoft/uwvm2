/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-05
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
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include "funcbody.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::section
{
    /// @brief      Sections
    /// @details    Each section consists of a one-byte section id
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.5.2
    enum class section_id : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
    {
        custom_sec = 0u,
        type_sec = 1u,
        import_sec = 2u,
        function_sec = 3u,
        table_sec = 4u,
        memory_sec = 5u,
        global_sec = 6u,
        export_sec = 7u,
        start_sec = 8u,
        element_sec = 9u,
        code_sec = 10u,
        data_sec = 11u,
    };

    /// @brief      Range for surface sections
    /// @details    Used to mark the corresponding range of wasm files
    struct section_span_view
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const* sec_begin{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const* sec_end{};
    };

    /// @brief      Custom Section
    /// @details    Custom sections have the id 0. They are intended to be used for debugging information or third-party extensions,
    ///             and are ignored by the WebAssembly semantics. Their contents consist of a name further identifying the custom
    ///             section, followed by an uninterpreted sequence of bytes for custom use.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.5.3
    struct custom_section
    {
        section_span_view sec_span{};
        ::uwvm2::utils::container::u8string_view custom_name{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const* custom_begin{};
        // please use sec_span.sec_end instead custom_end.
    };

    /// @brief      Type Section
    /// @details    The type section has the id 1. It decodes into a vector of function types that represent the types component of a module.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.5.4
    struct type_section
    {
        section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::function_type> types{};
    };

    /// @brief      Import Section
    /// @details    The import section has the id 2. It decodes into a vector of imports that represent the imports component of a module
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.5.5
    struct import_section
    {
        section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::import_type> importsec{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::import_type const*> importdesc_func_types{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::import_type const*> importdesc_table_types{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::import_type const*> importdesc_memory_types{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::import_type const*> importdesc_global_types{};
    };

    /// @brief      Function Section
    /// @details    The function section has the id 3. It decodes into a vector of type indices that represent the type fields of the
    ///             functions in the funcs component of a module. The locals and body fields of the respective functions are encoded
    ///             separately in the code section.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.5.6
    struct function_section
    {
        section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::local_function_type> funcsec{};
    };

    /// @brief      Table Section
    /// @details    The table section has the id 4. It decodes into a vector of tables that represent the tables component of a module.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.5.7
    struct table_section
    {
        section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::table_type> tablesec{};
    };

    /// @brief      Memory Section
    /// @details    The memory section has the id 5. It decodes into a vector of memories that represent the mems component of a module.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.5.8
    struct memory_section
    {
        section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::memory_type> memsec{};
    };

    /// @brief      Global Section
    /// @details    The global section has the id 6. It decodes into avector of globals that represent the globals component of a module.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.5.9
    struct global_section
    {
        section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::local_global_type> globalsec{};
    };

    /// @brief      Export Section
    /// @details    The export section has the id 7. It decodes into a vector of exports that represent the exports component of a module.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.5.10
    struct export_section
    {
        section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::export_type> exportsec{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::export_type const*> exportdesc_func_types{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::export_type const*> exportdesc_table_types{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::export_type const*> exportdesc_memory_types{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::export_type const*> exportdesc_global_types{};
    };

    /// @brief      Start Section
    /// @details    The start section has the id 8. It decodes into an optional start function that represents the start component of a module.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.5.11
    struct start_section
    {
        section_span_view sec_span{};

        ::uwvm2::parser::wasm::standard::wasm1::type::indices start{};
    };

    /// @brief      Element Section
    /// @details    The element section has the id 9. It decodes into a vector of element segments that represent the elem component
    ///             of a module.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.5.12
    struct element_section
    {
        section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::elem_segment_type> elemsec{};
    };

    /// @brief      Code Section
    /// @details    The code section has the id 10. It decodes into a vector of code entries that are pairs of value type vectors and
    ///             expressions. They represent the locals and body field of the functions in the funcs component of a module. The
    ///             type fields of the respective functions are encoded separately in the function section.
    ///             No ast conversion here, would be a waste of time
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.5.16
    struct code_section
    {
        section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::section::code_func_body> codesec{};
    };

    /// @brief      Data Section
    /// @details    The data section has the id 11. It decodes into a vector of data segments that represent the data component of a module.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.5.14
    struct data_section
    {
        section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::data_segment> datasec{};
    };
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::section::custom_section>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::parser::wasm::standard::wasm1::section::custom_section>);
}
