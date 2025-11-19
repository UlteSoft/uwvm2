# UWVM2 Wasm Parser Concepts and ADL Map

This page provides a high-level, concept-first overview of UWVM2's extensible WebAssembly parser. It focuses on how concepts compose the parser at compile time, and how ADL (Argument-Dependent Lookup) is used as the type-safe extension mechanism. The diagram below is a birds-eye view: core concepts → binfmt v1 section concepts → wasm1 standard features and their ADL handlers.

Note: All code examples are minimal and illustrative. They compile in freestanding environments if the surrounding headers are available.

@dot
digraph G {
  rankdir=LR;
  graph [compound=true, fontsize=10, fontname="Helvetica", fontcolor="#111111", color="#666666", bgcolor="transparent"];
  node  [shape=box, fontsize=9, fontname="Helvetica", fontcolor="#222222", color="#666666", style="filled", fillcolor="white"];
  edge  [fontsize=8, fontname="Helvetica", fontcolor="#222222", color="#666666"];

  subgraph cluster_core {
    label="core concepts";
    color=lightgrey;
    wasm_feature [label="concept wasm_feature\n= has_feature_name & has_wasm_binfmt_version & is_empty"];
    has_feature_name [label="concept has_feature_name\nFeatureType::feature_name : u8string_view"];
    has_wasm_binfmt_version [label="concept has_wasm_binfmt_version\nFeatureType::binfmt_version : wasm_u32 > 0"];
    has_wasm_binfmt_parsering_strategy [label="concept has_wasm_binfmt_parsering_strategy\nrequires ADL define_wasm_binfmt_parsering_strategy(...) → func_p"];
    feature_parameter [label="feature_parameter_t<Fs...>\ncompile-time parameters tuple"];
    check_duplicate_binfmt [label="operation::check_has_duplicate_binfmt_handler<Fs...>\ncompile-time validation"];

    wasm_feature -> has_feature_name;
    wasm_feature -> has_wasm_binfmt_version;
    check_duplicate_binfmt -> has_wasm_binfmt_parsering_strategy [label="ver handler must exist per version"];
  }

  subgraph cluster_binfmt1 {
    label="binfmt ver1 sections";
    color=lightgrey;
    has_binfmt_ver1_section_define [label="concept has_binfmt_ver1_section_define\nFeature::binfmt_ver1_section_type is tuple"];
    has_binfmt_ver1_extensible_section_define [label="concept has_binfmt_ver1_extensible_section_define\nFeature::template binfmt_ver1_section_type<Fs...> is tuple"];
    has_section_id_define [label="concept has_section_id_define\nSec::section_id : wasm_byte"];
    has_section_name_define [label="concept has_section_name_define\nSec::section_name : u8string_view"];
    has_handle_binfmt_ver1_extensible_section_define [label="concept has_handle_binfmt_ver1_extensible_section_define\nrequires ADL handle_binfmt_ver1_extensible_section_define(...)"];     
    has_sid_and_handle [label="concept has_section_id_and_handle_binfmt_ver1_extensible_section_define\n= id + handler"];
    has_final_check [label="concept has_final_check\nFeature::final_check is type_replacer<...>"];
    has_final_check_handler [label="concept has_final_check_handler\nrequires ADL define_final_check(...)"];
    has_section_sequential_packer [label="concept has_section_sequential_packer\nFeature::section_sequential_packer is type_replacer"];
    has_section_id_sequential_mapping_table_define [label="concept has_section_id_sequential_mapping_table_define\nfinal_section_sequential_packer_t<Fs...>::section_id_sequential_mapping_table"];

    has_sid_and_handle -> has_section_id_define;
    has_sid_and_handle -> has_handle_binfmt_ver1_extensible_section_define;
    has_section_id_sequential_mapping_table_define -> has_section_sequential_packer [style=dashed];
  }

  subgraph cluster_wasm1_types {
    label="wasm1 type section";
    color=lightgrey;
    has_type_type [label="concept has_type_type\nFeature::type_type<Fs...> is type_replacer"];
    has_value_type [label="concept has_value_type\nFeature::value_type is type_replacer"];
    has_type_prefix [label="concept has_type_prefix\nFeature::type_prefix is type_replacer"];
    has_check_typesec_value_type [label="concept has_check_typesec_value_type\nADL define_check_typesec_value_type(...): bool"];
    has_type_prefix_handler [label="concept has_type_prefix_handler\nADL define_type_prefix_handler(...): byte*"];
    has_check_duplicate_types [label="concept has_prohibit_duplicate_types\nFeature::prohibit_duplicate_types : bool"];
    has_check_duplicate_types_handler [label="concept has_check_duplicate_types_handler\nADL define_check_duplicate_types(...)"];
    allow_multi_result [label="concept has_allow_multi_result_vector\nFsCurr::allow_multi_result_vector : bool"];
    allow_multi_result_para [label="has_feature_parameter_controllable_allow_multi_result_vector_from_paras_c\n(runtime override)"];
    allow_multi_result_curr [label="has_curr_feature_parameter_controllable_allow_multi_result_vector\nCurrParaType::controllable_allow_multi_result_vector : bool"];

    has_type_prefix_handler -> has_type_prefix [style=dashed,label="ADL depends on prefix type"];
    has_check_duplicate_types_handler -> has_check_duplicate_types [style=dashed,label="enabled if true"];
    allow_multi_result_para -> allow_multi_result_curr [style=dotted, label="from paras"];
  }

  subgraph cluster_import {
    label="wasm1 import section";
    color=lightgrey;
    has_text_format [label="concept has_text_format\nFeature::text_format is type_replacer"];
    has_extern_type [label="concept has_extern_type\nFeature::extern_type<Fs...> is type_replacer"];
    final_extern_valid [label="concept is_valid_final_extern_type_t<Fs...>"];
    has_disable_zero_length_string [label="concept has_disable_zero_length_string\nFsCurr::disable_zero_length_string : bool"];
    has_check_duplicate_imports [label="concept has_check_duplicate_imports\nFsCurr::check_duplicate_imports : bool"];
    check_duplicate_imports_para [label="has_feature_parameter_controllable_check_duplicate_imports_from_paras_c\n(runtime override)"];
    has_curr_check_duplicate_imports [label="has_curr_feature_parameter_controllable_check_duplicate_imports\nCurrParaType::controllable_check_duplicate_imports : bool"];
    has_extern_prefix_imports_handler [label="concept has_extern_prefix_imports_handler\nADL define_extern_prefix_imports_handler(...): byte*"];
    has_extern_imports_table_handler [label="concept has_extern_imports_table_handler\nADL extern_imports_table_handler(...): byte*"];
    has_extern_imports_memory_handler [label="concept has_extern_imports_memory_handler\nADL extern_imports_memory_handler(...): byte*"];
    has_extern_imports_global_handler [label="concept has_extern_imports_global_handler\nADL extern_imports_global_handler(...): byte*"];
    can_check_import_export_text_format [label="concept can_check_import_export_text_format\nADL define_check_import_export_text_format(...): bool"];

    final_extern_valid -> has_extern_type;
    can_check_import_export_text_format -> has_text_format [style=dotted, label="uses"];
    check_duplicate_imports_para -> has_curr_check_duplicate_imports [style=dotted, label="from paras"];
  }

  subgraph cluster_function_section {
    label="wasm1 function section";
    color=lightgrey;
    function_section [label="function_section_storage_t\n(typeidx vector, depends on type section)"];
  }

  subgraph cluster_table_section {
    label="wasm1 table section";
    color=lightgrey;
    has_table_type [label="concept has_table_type\nFeature::table_type is type_replacer"];
    has_allow_multi_table [label="concept has_allow_multi_table\nFsCurr::allow_multi_table : bool"];
    has_table_section_table_handler [label="concept has_table_section_table_handler\nADL table_section_table_handler(...): byte*"];
  }

  subgraph cluster_memory_section {
    label="wasm1 memory section";
    color=lightgrey;
    has_memory_type [label="concept has_memory_type\nFeature::memory_type is type_replacer"];
    has_allow_multi_memory [label="concept has_allow_multi_memory\nFsCurr::allow_multi_memory : bool"];
    has_memory_section_memory_handler [label="concept has_memory_section_memory_handler\nADL memory_section_memory_handler(...): byte*"];
  }

  subgraph cluster_global_section {
    label="wasm1 global section";
    color=lightgrey;
    has_global_type [label="concept has_global_type\nFeature::global_type is type_replacer"];
    has_wasm_const_expr [label="concept has_wasm_const_expr\nFeature::wasm_const_expr is type_replacer"];
    has_global_section_global_handler [label="concept has_global_section_global_handler\nADL global_section_global_handler(...): byte*"];
    has_parse_and_check_global_expr_valid [label="concept has_parse_and_check_global_expr_valid\nADL parse_and_check_global_expr_valid(...): byte*"];
    has_global_section_global_handler -> has_global_type [style=dashed];
    has_parse_and_check_global_expr_valid -> has_wasm_const_expr [style=dashed];
  }

  subgraph cluster_export_section {
    label="wasm1 export section";
    color=lightgrey;
    has_export_type [label="concept has_export_type\nFeature::export_type<Fs...> is type_replacer"];
    is_valid_final_export_type_t [label="concept is_valid_final_export_type_t<Fs...>"];
    has_handle_export_index [label="concept has_handle_export_index\nADL define_handle_export_index(...): byte*"];
    is_valid_final_export_type_t -> has_export_type;
  }

  subgraph cluster_start_section {
    label="wasm1 start section";
    color=lightgrey;
    start_section [label="start_section_storage_t\n(ADL handle_binfmt_ver1_extensible_section_define)"];
  }

  subgraph cluster_element_section {
    label="wasm1 element section";
    color=lightgrey;
    has_element_type [label="concept has_element_type\nFeature::element_type<Fs...> is type_replacer"];
    is_valid_final_element_type_t [label="concept is_valid_final_element_type_t<Fs...>"];
    has_handle_element_type [label="concept has_handle_element_type\nADL define_handler_element_type(...): byte*"];
    is_valid_final_element_type_t -> has_element_type;
  }

  subgraph cluster_code_section {
    label="wasm1 code section";
    color=lightgrey;
    has_check_codesec_value_type [label="concept has_check_codesec_value_type\nADL define_check_codesec_value_type(...): bool"];
  }

  subgraph cluster_data_section {
    label="wasm1 data section";
    color=lightgrey;
    has_data_type [label="concept has_data_type\nFeature::data_type<Fs...> is type_replacer"];
    is_valid_final_data_type_t [label="concept is_valid_final_data_type_t<Fs...>"];
    has_handle_data_type [label="concept has_handle_data_type\nADL define_handler_data_type(...): byte*"];
    is_valid_final_data_type_t -> has_data_type;
  }

  subgraph cluster_custom_sections {
    label="custom sections";
    color=lightgrey;
    has_custom_section_sequential_mapping_table_define [label="concept has_custom_section_sequential_mapping_table_define\ncompile-time name→order hash table"];
  }

  // Cross-cluster influences
  has_extern_prefix_imports_handler -> can_check_import_export_text_format [style=dotted, label="name check (optional)"];
  has_handle_export_index -> can_check_import_export_text_format [style=dotted];
  // Core helpers
  check_is_type_replacer [label="operation::details::check_is_type_replacer\nvalidate type_replacer signature"];
  has_feature_parameter [label="concept has_feature_parameter\nFc::parameter exists"];
}
@enddot

## Concept Index with Purpose and Examples

Below is a concise catalog of the concepts shown above. For each item: purpose, how to use, and a short example.

### Core

- Name: has_feature_name
  - Purpose: A feature must declare a static `u8string_view` name to be discoverable.
  - Usage: Gate type-level identity and runtime prints.
  - Example:
    ```cpp
    struct my_feature {
      inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"my_feature"};
    };
    static_assert(::uwvm2::parser::wasm::concepts::has_feature_name<my_feature>);
    ```

- Name: has_wasm_binfmt_version
  - Purpose: Bind a feature to a specific wasm binary format version (non-zero `wasm_u32`).
  - Usage: Drives versioned parsing strategy composition.
  - Example:
    ```cpp
    struct my_feature {
      inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};
    };
    static_assert(::uwvm2::parser::wasm::concepts::has_wasm_binfmt_version<my_feature>);
    ```

- Name: has_wasm_binfmt_parsering_strategy
  - Purpose: Provide an ADL function `define_wasm_binfmt_parsering_strategy(feature_reserve_type_t<F>, tuple<Fs...>)` returning the handler func pointer type.
  - Usage: Central ADL entry to map (Fs...) to a concrete version handler function pointer.
  - Example:
    ```cpp
    // ADL in same namespace as feature F
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr auto define_wasm_binfmt_parsering_strategy(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<my_feature>,
        ::uwvm2::utils::container::tuple<Fs...>) {
      using module_storage_t = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>;
      return ::uwvm2::parser::wasm::concepts::binfmt_handle_version_func_p_type<module_storage_t, Fs...>{};
    }
    static_assert(::uwvm2::parser::wasm::concepts::has_wasm_binfmt_parsering_strategy<my_feature>);
    ```

- Name: operation::check_has_duplicate_binfmt_handler
  - Purpose: Compile-time validation that required version handlers exist exactly once.
  - Usage: `check_has_duplicate_binfmt_handler<Fs...>();` inside version synthesis.

- Name: operation::details::check_is_type_replacer
  - Purpose: Ensure a feature-provided alias matches `type_replacer<superseded,replacement>` shape.

- Name: has_feature_parameter
  - Purpose: A feature declares `using parameter = ...;` to enable runtime controls aggregation.

### Binfmt ver1 sections

- Name: has_binfmt_ver1_section_define
  - Purpose: A feature can provide `binfmt_ver1_section_type` tuple to splice sections.
  - Example:
    ```cpp
    struct my_feature {
      using binfmt_ver1_section_type = ::uwvm2::utils::container::tuple<type_section, import_section /* ... */>;
    };
    static_assert(::uwvm2::parser::wasm::binfmt::ver1::has_binfmt_ver1_section_define<my_feature>);
    ```

- Name: has_binfmt_ver1_extensible_section_define
  - Purpose: A feature can provide templated `binfmt_ver1_section_type<Fs...>` to splice sections with awareness of `Fs...`.

- Name: has_section_id_define / has_section_name_define
  - Purpose: Each section declares `section_id` and optional `section_name`.
  - Example:
    ```cpp
    struct type_section { inline static constexpr wasm_byte section_id{1}; };
    static_assert(::uwvm2::parser::wasm::binfmt::ver1::has_section_id_define<type_section>);
    ```

- Name: has_handle_binfmt_ver1_extensible_section_define
  - Purpose: ADL `handle_binfmt_ver1_extensible_section_define(...)` handles a concrete section.
  - Example:
    ```cpp
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    void handle_binfmt_ver1_extensible_section_define(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<type_section>,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_begin,
        ::std::byte const* section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
        ::std::byte const* sec_id_module_ptr)
    {}
    ```

- Name: has_section_id_and_handle_binfmt_ver1_extensible_section_define
  - Purpose: Composition: a section must provide both `section_id` and its ADL handler.

- Name: has_final_check / has_final_check_handler
  - Purpose: An optional final pass via `Feature::final_check` and ADL `define_final_check(...)`.

- Name: has_section_sequential_packer
  - Purpose: Provide compile-time section ordering strategy via `Feature::section_sequential_packer`.

- Name: has_section_id_sequential_mapping_table_define
  - Purpose: Provide `section_id_sequential_mapping_table` for fixed standard sections.

### Wasm1 type section

- Name: has_type_type / has_value_type / has_type_prefix
  - Purpose: Feature-provided replacement hooks for composition of type information.
  - Example:
    ```cpp
    struct F { template <typename... Fs> using type_type = type_replacer<root_of_replacement, my_type<Fs...>>; };
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_type_type<F, void>);
    ```

- Name: has_check_typesec_value_type
  - Purpose: ADL `define_check_typesec_value_type(sec_adl, final_value_type)` returns bool.

- Name: has_type_prefix_handler
  - Purpose: ADL `define_type_prefix_handler(...)` parses type prefix and advances the cursor.

- Name: has_prohibit_duplicate_types + has_check_duplicate_types_handler
  - Purpose: Compile-time switch + ADL implementation for duplicate type detection.

- Name: has_allow_multi_result_vector + controllable override
  - Purpose: Allow multiple results at compile-time; runtime override via `feature_parameter_t`.

- Name: has_curr_feature_parameter_controllable_allow_multi_result_vector
  - Purpose: The per-parameter flag enabling runtime control for multi-results.

### Wasm1 import section

- Name: has_extern_type + is_valid_final_extern_type_t
  - Purpose: Compose final extern variants; validate layout and enum types.

- Name: has_disable_zero_length_string
  - Purpose: Optional policy to disallow empty names at parse-time.

- Name: has_check_duplicate_imports + controllable override
  - Purpose: Prohibit duplicate import entries; allow runtime override via parameters.

- Name: has_text_format
  - Purpose: Provide text-format enum wrapper used by name validators.

- Name: has_extern_prefix_imports_handler / has_extern_imports_*_handler
  - Purpose: ADL entry points to parse per-kind import payloads.

- Name: can_check_import_export_text_format
  - Purpose: Optional ADL name validator shared by import/export sections.
  - Example:
    ```cpp
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    bool define_check_import_export_text_format(
        ::uwvm2::parser::wasm::standard::wasm1::features::final_text_format_wapper<Fs...>,
        ::std::byte const* begin,
        ::std::byte const* end,
        ::uwvm2::parser::wasm::base::error_impl& err)
    { return true; }
    ```

- Name: has_curr_feature_parameter_controllable_check_duplicate_imports
  - Purpose: The per-parameter flag enabling runtime control for duplicate-import checks.

### Wasm1 function section

- Purpose: Store type indices for defined functions, minimized by storage modes; depends on type section contents.

### Wasm1 table section

- Name: has_table_type
  - Purpose: Feature-provided table type replacement.
- Name: has_allow_multi_table
  - Purpose: Allow multiple tables.
- Name: has_table_section_table_handler
  - Purpose: ADL table handler.
  - Example:
    ```cpp
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    ::std::byte const* table_section_table_handler(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::features::final_table_type<Fs...>& table_r,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para)
    { return section_curr; }
    ```

### Wasm1 memory section

- Name: has_memory_type
  - Purpose: Feature-provided memory type replacement.
- Name: has_allow_multi_memory
  - Purpose: Allow multiple memories.
- Name: has_memory_section_memory_handler
  - Purpose: ADL memory handler.
  - Example:
    ```cpp
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    ::std::byte const* memory_section_memory_handler(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::features::final_memory_type<Fs...>& memory_r,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para)
    { return section_curr; }
    ```

### Wasm1 global section

- Name: has_global_type
  - Purpose: Feature-provided global type replacement.
- Name: has_wasm_const_expr
  - Purpose: Compose const-expression checker/type.
- Name: has_global_section_global_handler / has_parse_and_check_global_expr_valid
  - Purpose: ADL parse + init-expr validation.
  - Example:
    ```cpp
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    ::std::byte const* global_section_global_handler(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::features::final_global_type<Fs...>& global_r,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para)
    { return section_curr; }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    ::std::byte const* parse_and_check_global_expr_valid(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::features::final_global_type<Fs...> const& global_r,
        ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...>& global_expr,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para)
    { return section_curr; }
    ```

### Wasm1 export section

- Name: has_export_type + is_valid_final_export_type_t + has_handle_export_index
  - Purpose: Compose/validate export type; ADL index handler.
  - Example:
    ```cpp
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    ::std::byte const* define_handle_export_index(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::export_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::features::final_export_type_t<Fs...>& fwet,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para)
    { return section_curr; }
    ```

### Wasm1 start section

- Purpose: Holds start function index; parsed via ADL `handle_binfmt_ver1_extensible_section_define` for start_section.

### Wasm1 element section

- Name: has_element_type + is_valid_final_element_type_t + has_handle_element_type
  - Purpose: Compose/validate element type; ADL handler.
  - Example:
    ```cpp
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    ::std::byte const* define_handler_element_type(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::features::final_element_type_t<Fs...>& fet,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para)
    { return section_curr; }
    ```

### Wasm1 code section

- Name: has_check_codesec_value_type
  - Purpose: ADL value_type checker for code bodies.
  - Example:
    ```cpp
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    bool define_check_codesec_value_type(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...> fvt)
    { return true; }
    ```

### Wasm1 data section

- Name: has_data_type + is_valid_final_data_type_t + has_handle_data_type
  - Purpose: Compose/validate data type; ADL handler.

---

Implementation notes:
- ADL entry functions must be discoverable via the same namespace as their `feature_reserve_type_t<...>` or section storage wrappers.
- All composition happens at compile time; validation is enforced with `static_assert` and constrained templates.


