// Runtime declaration view for fused WebAssembly 3 compilation and validation-only APIs.
// The Core 3 type rules live in validation/standard/wasm3 (see
// https://webassembly.github.io/spec/core/appendix/algorithm.html).
// Int full and LLVM full deliberately include this fragment in their details namespaces.
// Normal full/lazy compilation checks instruction semantics while emitting code.
// This adapter checks metadata and exposes declarations; it does not scan function bodies.
// No include guard: both compiler namespaces may be instantiated in one translation unit.

    template <typename FeatureTuple>
    struct validation_module_traits;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct validation_module_traits<::uwvm2::utils::container::tuple<Fs...>>
    {
        using module_storage_t = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>;
        using import_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>;
        using type_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>;
        using function_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t;
        using code_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>;
        using table_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>;
        using memory_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>;
        using global_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>;
        using export_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::export_section_storage_t<Fs...>;
        using element_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>;
        using data_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::data_section_storage_t<Fs...>;
        using data_count_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>;
        using tag_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1p1::features::tag_section_storage_t<Fs...>;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;
    };

    using validation_module_traits_t = validation_module_traits<::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_features_t>;
    using validation_module_storage_t = validation_module_traits_t::module_storage_t;

    [[noreturn]] inline constexpr void runtime_storage_bug() noexcept
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
        ::fast_io::fast_terminate();
    }

    [[nodiscard]] inline constexpr validation_module_traits_t::wasm_u32 checked_cast_size_to_wasm_u32(::std::size_t sz) noexcept
    {
        using wasm_u32 = validation_module_traits_t::wasm_u32;
        if(sz > static_cast<::std::size_t>(::std::numeric_limits<wasm_u32>::max())) [[unlikely]] { runtime_storage_bug(); }
        return static_cast<wasm_u32>(sz);
    }

    [[nodiscard]] inline constexpr validation_module_traits_t::wasm_u32
        checked_runtime_index_space_count(::std::size_t imported, ::std::size_t local) noexcept
    {
        using wasm_u32 = validation_module_traits_t::wasm_u32;
        auto const limit{static_cast<::std::size_t>((::std::numeric_limits<wasm_u32>::max)())};
        if(imported > limit || local > limit - imported) [[unlikely]] { runtime_storage_bug(); }
        return static_cast<wasm_u32>(imported + local);
    }

    [[nodiscard]] inline constexpr ::std::size_t
        get_runtime_type_section_count(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module) noexcept
    {
        auto const type_begin{curr_module.type_section_storage.type_section_begin};
        auto const type_end{curr_module.type_section_storage.type_section_end};

        if(type_begin == nullptr || type_end == nullptr) [[unlikely]]
        {
            if(type_begin != type_end) [[unlikely]] { runtime_storage_bug(); }
            return 0uz;
        }

        auto const type_begin_addr{reinterpret_cast<::std::uintptr_t>(type_begin)};
        auto const type_end_addr{reinterpret_cast<::std::uintptr_t>(type_end)};
        if(type_begin_addr > type_end_addr) [[unlikely]] { runtime_storage_bug(); }

        constexpr ::std::size_t type_size{sizeof(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t)};
        static_assert(type_size != 0uz);

        auto const byte_span{type_end_addr - type_begin_addr};
        if(byte_span % type_size != 0uz) [[unlikely]] { runtime_storage_bug(); }
        return byte_span / type_size;
    }

    // Only stricter GC declaration policy consumes this parser-owned borrow.
    // The initializer publishes the actual context object even for the legacy
    // 0x60 function-only representation. Its retained parser-module owner keeps
    // this object's address stable; a missing or mismatched borrow is not proof.
    [[nodiscard]] inline constexpr ::uwvm2::validation::standard::wasm3::recursive_type_context const*
        runtime_declaration_type_context(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module) noexcept
    {
        auto const& types{curr_module.type_section_storage};
        auto const count{get_runtime_type_section_count(curr_module)};
        if(count != types.type_section_count) [[unlikely]] { runtime_storage_bug(); }
        auto const* context{types.core3_declaration_context_ptr};
        if(context == nullptr) { return nullptr; }
        // This borrow was published by the initializer from the module's retained
        // parser type section. The context object's address, not just its records'
        // allocation, obeys the existing parser/signature compilation lifetime.
        // No guest token or arbitrary missing-context/count pair creates it.
        if(context->records.empty())
        {
            if(types.requires_gc || types.core3_recursive_types_ptr != nullptr ||
               types.core3_context_ptr != nullptr) { return nullptr; }
        }
        else if(context != types.core3_context_ptr || types.core3_recursive_types_ptr == nullptr ||
                context->records.size() != count || types.core3_recursive_types_ptr->type_count != count)
        { return nullptr; }
        return context;
    }

    [[nodiscard]] inline constexpr validation_module_traits_t::wasm_u32
        get_runtime_type_index(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                               ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* function_type_ptr) noexcept
    {
        if(function_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }

        auto const type_begin{curr_module.type_section_storage.type_section_begin};
        auto const type_count{get_runtime_type_section_count(curr_module)};
        if(type_count == 0uz || type_begin == nullptr) [[unlikely]] { runtime_storage_bug(); }

        auto const type_begin_addr{reinterpret_cast<::std::uintptr_t>(type_begin)};
        auto const function_type_addr{reinterpret_cast<::std::uintptr_t>(function_type_ptr)};
        auto const type_end_addr{type_begin_addr + type_count * sizeof(*type_begin)};

        if(function_type_addr < type_begin_addr || function_type_addr >= type_end_addr) [[unlikely]] { runtime_storage_bug(); }

        auto const byte_offset{function_type_addr - type_begin_addr};
        if(byte_offset % sizeof(*type_begin) != 0uz) [[unlikely]] { runtime_storage_bug(); }

        return checked_cast_size_to_wasm_u32(byte_offset / sizeof(*type_begin));
    }

    inline constexpr void validate_runtime_module_storage(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module) noexcept
    {
        using external_types = validation_module_traits_t::external_types;

        static_cast<void>(checked_runtime_index_space_count(curr_module.imported_function_vec_storage.size(),
                                                           curr_module.local_defined_function_vec_storage.size()));
        static_cast<void>(checked_runtime_index_space_count(curr_module.imported_table_vec_storage.size(),
                                                           curr_module.local_defined_table_vec_storage.size()));
        static_cast<void>(checked_runtime_index_space_count(curr_module.imported_memory_vec_storage.size(),
                                                           curr_module.local_defined_memory_vec_storage.size()));
        static_cast<void>(checked_runtime_index_space_count(curr_module.imported_global_vec_storage.size(),
                                                           curr_module.local_defined_global_vec_storage.size()));
        static_cast<void>(checked_runtime_index_space_count(curr_module.imported_tag_vec_storage.size(),
                                                           curr_module.local_defined_tag_vec_storage.size()));
        static_cast<void>(checked_cast_size_to_wasm_u32(get_runtime_type_section_count(curr_module)));

        for(auto const& imported_func: curr_module.imported_function_vec_storage)
        {
            auto const import_type_ptr{imported_func.import_type_ptr};
            if(import_type_ptr == nullptr || import_type_ptr->imports.type != external_types::func || import_type_ptr->imports.storage.function == nullptr)
                [[unlikely]]
            {
                runtime_storage_bug();
            }
            static_cast<void>(get_runtime_type_index(curr_module, import_type_ptr->imports.storage.function));
        }

        for(auto const& local_func: curr_module.local_defined_function_vec_storage)
        {
            if(local_func.function_type_ptr == nullptr || local_func.wasm_code_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
            static_cast<void>(get_runtime_type_index(curr_module, local_func.function_type_ptr));

            auto const code_begin{reinterpret_cast<::std::byte const*>(local_func.wasm_code_ptr->body.expr_begin)};
            auto const code_end{reinterpret_cast<::std::byte const*>(local_func.wasm_code_ptr->body.code_end)};
            if(code_begin == nullptr || code_end == nullptr || code_begin > code_end) [[unlikely]] { runtime_storage_bug(); }
        }

        for(auto const& imported_table: curr_module.imported_table_vec_storage)
        {
            auto const import_type_ptr{imported_table.import_type_ptr};
            if(import_type_ptr == nullptr || import_type_ptr->imports.type != external_types::table) [[unlikely]] { runtime_storage_bug(); }
        }
        for(auto const& local_table: curr_module.local_defined_table_vec_storage)
        {
            if(local_table.table_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
        }

        for(auto const& imported_memory: curr_module.imported_memory_vec_storage)
        {
            auto const import_type_ptr{imported_memory.import_type_ptr};
            if(import_type_ptr == nullptr || import_type_ptr->imports.type != external_types::memory) [[unlikely]] { runtime_storage_bug(); }
        }
        for(auto const& local_memory: curr_module.local_defined_memory_vec_storage)
        {
            if(local_memory.memory_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
        }

        for(auto const& imported_global: curr_module.imported_global_vec_storage)
        {
            auto const import_type_ptr{imported_global.import_type_ptr};
            if(import_type_ptr == nullptr || import_type_ptr->imports.type != external_types::global) [[unlikely]] { runtime_storage_bug(); }
        }
        for(auto const& local_global: curr_module.local_defined_global_vec_storage)
        {
            if(local_global.global_type_ptr == nullptr || local_global.local_global_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }

            auto const& global_type{*local_global.global_type_ptr};
            auto const& local_global_type{local_global.local_global_type_ptr->global};
            if(global_type.type != local_global_type.type || global_type.is_mutable != local_global_type.is_mutable) [[unlikely]]
            {
                runtime_storage_bug();
            }
        }

        for(auto const& imported_tag: curr_module.imported_tag_vec_storage)
        {
            auto const import_type_ptr{imported_tag.import_type_ptr};
            if(import_type_ptr == nullptr || import_type_ptr->imports.type != external_types::tag ||
               imported_tag.function_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
            if(get_runtime_type_index(curr_module, imported_tag.function_type_ptr) != import_type_ptr->imports.storage.tag_type_index)
                [[unlikely]] { runtime_storage_bug(); }
        }
        for(auto const& local_tag: curr_module.local_defined_tag_vec_storage)
        {
            if(local_tag.function_type_ptr == nullptr ||
               get_runtime_type_index(curr_module, local_tag.function_type_ptr) != local_tag.type_index)
                [[unlikely]] { runtime_storage_bug(); }
        }
    }

    [[nodiscard]] inline constexpr ::uwvm2::validation::standard::wasm3::core3_exception_declaration_requirements
        runtime_exception_declaration_requirements(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module) noexcept
    {
        return {.types = curr_module.type_section_storage.requires_exceptions,
                .tables = curr_module.table_declarations_require_exceptions,
                .globals = curr_module.global_declarations_require_exceptions,
                .elements = curr_module.element_declarations_require_exceptions,
                .locals = curr_module.code_declarations_require_exceptions,
                .tags = curr_module.tag_section_present || !curr_module.imported_tag_vec_storage.empty()};
    }
    [[nodiscard]] inline constexpr ::uwvm2::validation::standard::wasm3::core3_storage_declaration_requirements
        runtime_storage_declaration_requirements(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module) noexcept
    {
        return {.tables_reference_types = curr_module.table_declarations_require_reference_types,
                .globals_reference_types = curr_module.global_declarations_require_reference_types,
                .elements_reference_types = curr_module.element_declarations_require_reference_types,
                .globals_simd = curr_module.global_declarations_require_simd,
                .table_reference_value = curr_module.table_reference_types_diagnostic_value,
                .global_reference_value = curr_module.global_reference_types_diagnostic_value,
                .element_reference_value = curr_module.element_reference_types_diagnostic_value};
    }
    [[nodiscard]] inline constexpr ::uwvm2::validation::standard::wasm3::core3_address_declaration_requirements
        runtime_address_declaration_requirements(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module) noexcept
    {
        return {.memory64 = curr_module.memory_declarations_require_memory64,
                .table64 = curr_module.table_declarations_require_table64,
                .shared = curr_module.memory_declarations_require_threads,
                .multi_memory = curr_module.memory_declarations_require_multi_memory};
    }
    [[nodiscard]] inline constexpr ::uwvm2::validation::standard::wasm3::core3_constant_declaration_requirements
        runtime_constant_declaration_requirements(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module) noexcept
    {
        return {.table_initializer = curr_module.table_declarations_require_table_initializer,
                .extended_const = curr_module.constant_expressions_require_extended_const,
                .extended_const_value = curr_module.extended_const_diagnostic_value,
                .extended_const_subject = curr_module.extended_const_diagnostic_subject,
                .opcodes = curr_module.constant_expression_opcode_requirements};
    }
    template<typename Parameters>
    inline constexpr void require_runtime_module_declaration_policy(
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module, Parameters const& parameters,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        auto const& policy{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(parameters)};
        if(::uwvm2::validation::standard::wasm3::core3_declaration_policy_fully_enabled(policy)) { return; } // No fully-enabled declaration or body walk.
        ::std::byte const* diagnostic{};
        if(!curr_module.local_defined_function_vec_storage.empty())
        {
            // [module-owned local functions ...] end: nonempty proves index zero.
            // [safe                           ] only immutable metadata is read; no guest byte is accessed.
            auto const* code{curr_module.local_defined_function_vec_storage.index_unchecked(0uz).wasm_code_ptr};
            if(code == nullptr) [[unlikely]] { runtime_storage_bug(); }
            // [parser-proven expression ...] code_end
            // [safe                        ] diagnostic borrows the original body endpoint, without arithmetic.
            // ^^ diagnostic: a module without bodies retains null; neither value is dereferenced by the policy helper.
            diagnostic = reinterpret_cast<::std::byte const*>(code->body.expr_begin);
        }
        ::uwvm2::validation::standard::wasm3::require_module_declaration_policy(policy,
            {.types = curr_module.type_section_storage.requires_gc,
             .tables = curr_module.table_declarations_require_gc,
             .globals = curr_module.global_declarations_require_gc,
             .elements = curr_module.element_declarations_require_gc},
            runtime_exception_declaration_requirements(curr_module), diagnostic, err,
            {.types = curr_module.type_section_storage.requires_function_references,
             .tables = curr_module.table_declarations_require_function_references,
             .globals = curr_module.global_declarations_require_function_references,
             .elements = curr_module.element_declarations_require_function_references},
            {.simd = curr_module.type_section_storage.requires_simd,
             .reference_types = curr_module.type_section_storage.requires_reference_types,
             .multi_value = curr_module.type_section_storage.requires_multi_value},
            runtime_storage_declaration_requirements(curr_module), runtime_address_declaration_requirements(curr_module),
            runtime_constant_declaration_requirements(curr_module));
    }

    // Retain the already published EH spelling for callers with held source
    // candidates. It forwards to the same complete GC/EH module rule, without
    // performing a second metadata/body traversal.
    template<typename Parameters>
    inline constexpr void require_runtime_exception_declaration_policy(
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module, Parameters const& parameters,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    { require_runtime_module_declaration_policy(curr_module, parameters, err); }

    [[nodiscard]] inline constexpr validation_module_storage_t
        build_runtime_validation_module(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module) noexcept
    {
        validate_runtime_module_storage(curr_module);

        validation_module_storage_t validation_module{};

        auto& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::import_section_storage_t>(
            validation_module.sections)};
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::type_section_storage_t>(
            validation_module.sections)};
        auto& funcsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::function_section_storage_t>(
            validation_module.sections)};
        auto& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::code_section_storage_t>(
            validation_module.sections)};
        auto& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::table_section_storage_t>(
            validation_module.sections)};
        auto& memsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::memory_section_storage_t>(
            validation_module.sections)};
        auto& globalsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::global_section_storage_t>(
            validation_module.sections)};
        auto& exportsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::export_section_storage_t>(
            validation_module.sections)};
        auto& elemsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::element_section_storage_t>(
            validation_module.sections)};
        auto& datasec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::data_section_storage_t>(
            validation_module.sections)};
        auto& datacountsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::data_count_section_storage_t>(
            validation_module.sections)};

        auto& tagsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::tag_section_storage_t>(
            validation_module.sections)};
        // Revalidation must preserve declaration encoding policy even when no body uses that declaration.
        typesec.requires_function_references = curr_module.type_section_storage.requires_function_references;
        typesec.requires_gc = curr_module.type_section_storage.requires_gc;
        typesec.requires_exceptions = curr_module.type_section_storage.requires_exceptions;
        typesec.requires_simd = curr_module.type_section_storage.requires_simd;
        typesec.requires_reference_types = curr_module.type_section_storage.requires_reference_types;
        typesec.requires_multi_value = curr_module.type_section_storage.requires_multi_value;
        tablesec.requires_function_references = curr_module.table_declarations_require_function_references;
        globalsec.requires_function_references = curr_module.global_declarations_require_function_references;
        elemsec.requires_function_references = curr_module.element_declarations_require_function_references;
        tablesec.requires_gc = curr_module.table_declarations_require_gc;
        globalsec.requires_gc = curr_module.global_declarations_require_gc;
        elemsec.requires_gc = curr_module.element_declarations_require_gc;
        tablesec.requires_exceptions = curr_module.table_declarations_require_exceptions;
        globalsec.requires_exceptions = curr_module.global_declarations_require_exceptions;
        elemsec.requires_exceptions = curr_module.element_declarations_require_exceptions;
        tablesec.requires_reference_types = curr_module.table_declarations_require_reference_types;
        globalsec.requires_reference_types = curr_module.global_declarations_require_reference_types;
        elemsec.requires_reference_types = curr_module.element_declarations_require_reference_types;
        globalsec.requires_simd = curr_module.global_declarations_require_simd;
        tablesec.reference_types_diagnostic_value = curr_module.table_reference_types_diagnostic_value;
        globalsec.reference_types_diagnostic_value = curr_module.global_reference_types_diagnostic_value;
        elemsec.reference_types_diagnostic_value = curr_module.element_reference_types_diagnostic_value;
        memsec.requires_memory64 = curr_module.memory_declarations_require_memory64;
        memsec.requires_threads = curr_module.memory_declarations_require_threads;
        tablesec.requires_table64 = curr_module.table_declarations_require_table64;
        tablesec.requires_table_initializer = curr_module.table_declarations_require_table_initializer;
        globalsec.constant_expressions_require_extended_const = curr_module.constant_expressions_require_extended_const;
        globalsec.extended_const_diagnostic_value = curr_module.extended_const_diagnostic_value;
        globalsec.extended_const_diagnostic_subject = curr_module.extended_const_diagnostic_subject;
        globalsec.constant_expression_opcode_requirements = curr_module.constant_expression_opcode_requirements;
        codesec.locals_require_exceptions = curr_module.code_declarations_require_exceptions;
        tagsec.present = curr_module.tag_section_present;

        auto const type_count{get_runtime_type_section_count(curr_module)};
        typesec.types.reserve(type_count);
        auto const types_begin{curr_module.type_section_storage.type_section_begin};
        for(::std::size_t i{}; i != type_count; ++i)
        {
            // [types_begin, types_begin + type_count) is the checked runtime type range.
            // [safe                                      ] i < type_count proves this read.
            //         ^^ types_begin[i] is copied; no borrowed runtime cursor is advanced.
            typesec.types.push_back_unchecked(types_begin[i]);
        }
        // Runtime compilation first revalidates a synthetic parser module. Preserve the Core 3
        // function signatures as well as their flat execution carriers: otherwise a typed
        // parameter becomes an erased funcref and a valid call_ref is rejected before translation.
        auto const owned_begin{curr_module.type_section_storage.owned_signature_begin};
        auto const owned_end{curr_module.type_section_storage.owned_signature_end};
        if((owned_begin == nullptr) != (owned_end == nullptr)) [[unlikely]] { runtime_storage_bug(); }
        if(owned_begin != nullptr)
        {
            // [owned_begin, owned_end) is one retained parser allocation; both endpoints exist.
            // [safe                 ] subtraction is defined only after the null-pair check above.
            //                ^^ owned_end is one-past and is never dereferenced.
            if(static_cast<::std::size_t>(owned_end - owned_begin) != type_count) [[unlikely]] { runtime_storage_bug(); }
            typesec.owned_signatures.reserve(type_count);
            for(::std::size_t i{}; i != type_count; ++i)
            {
                // [owned_begin, owned_begin + type_count) is the retained signature range.
                // [safe                                      ] i < type_count proves this read.
                //         ^^ owned_begin[i] is copied into the synthetic module; no guest pointer escapes.
                auto const& signature{owned_begin[i]};
                if(signature.type_index != i) [[unlikely]] { runtime_storage_bug(); }
                typesec.owned_signatures.push_back_unchecked(signature);
                // The deep copy owns distinct carrier storage. Rebind this synthetic function
                // record to that copy before wasm3 validation reads its parameters or results.
                ::uwvm2::parser::wasm::standard::wasm3::type::bind_owned_function_signature(
                    typesec.owned_signatures.index_unchecked(i), typesec.types.index_unchecked(i));
            }
        }
        // The standard validator reads exact recursive definitions from this synthetic module.
        // Runtime storage borrows both objects from the same parser-owned, immutable type section.
        auto const* const recursive_types{curr_module.type_section_storage.core3_recursive_types_ptr};
        auto const* const recursive_context{curr_module.type_section_storage.core3_context_ptr};
        if((recursive_types == nullptr) != (recursive_context == nullptr)) [[unlikely]] { runtime_storage_bug(); }
        if(recursive_types != nullptr)
        {
            // [parser-owned recursive section/context] remain live for the runtime module lifetime.
            // [safe                                  ] no pointer arithmetic or cursor movement; deep copy below.
            // ^^ Both nonnull borrows are checked before dereference.
            if(recursive_types->type_count != type_count || recursive_context->records.size() != type_count)
                [[unlikely]] { runtime_storage_bug(); }
            typesec.core3_recursive_types = *recursive_types;
            typesec.core3_context = *recursive_context;
            typesec.core3_type_kinds.reserve(type_count);
            for(::std::size_t i{}; i != type_count; ++i)
            {
                // [records[0], records[type_count]) belongs to the checked retained context.
                // [safe                              ] i < type_count proves this read.
                //            ^^ index_unchecked(i) borrows one kind; no parser cursor advances.
                typesec.core3_type_kinds.push_back_unchecked(recursive_context->records.index_unchecked(i).kind);
            }
        }

        // A single parser import vector owns all five descriptor kinds. Check each
        // sum before reserve so push_back_unchecked cannot exceed that allocation.
        ::std::size_t total_import_count{};
        total_import_count = checked_runtime_index_space_count(total_import_count, curr_module.imported_function_vec_storage.size());
        total_import_count = checked_runtime_index_space_count(total_import_count, curr_module.imported_table_vec_storage.size());
        total_import_count = checked_runtime_index_space_count(total_import_count, curr_module.imported_memory_vec_storage.size());
        total_import_count = checked_runtime_index_space_count(total_import_count, curr_module.imported_global_vec_storage.size());
        total_import_count = checked_runtime_index_space_count(total_import_count, curr_module.imported_tag_vec_storage.size());
        importsec.imports.reserve(total_import_count);

        auto const append_runtime_import{[&]<validation_module_traits_t::external_types ExpectedKind>(
                                             ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t const* import_type_ptr) constexpr noexcept
                                             -> ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t const*
                                         {
                                             if(import_type_ptr == nullptr || import_type_ptr->imports.type != ExpectedKind) [[unlikely]]
                                             {
                                                 runtime_storage_bug();
                                             }

                                             importsec.imports.push_back_unchecked(*import_type_ptr);
                                             auto& copied_import{importsec.imports.back_unchecked()};

                                             if constexpr(ExpectedKind == validation_module_traits_t::external_types::func)
                                             {
                                                 auto const copied_func_type_ptr{copied_import.imports.storage.function};
                                                 if(copied_func_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
                                                 auto const type_index{get_runtime_type_index(curr_module, copied_func_type_ptr)};
                                                 // [typesec.types.cbegin(), typesec.types.cend()) owns type_count copied records.
                                                 // [safe                                     ] get_runtime_type_index proves type_index < type_count.
                                                 //          ^^ Rebind only this copied import; no runtime or guest cursor moves.
                                                 copied_import.imports.storage.function = typesec.types.cbegin() + type_index;
                                             }

                                             return ::std::addressof(copied_import);
                                         }};

        auto& imported_funcs{importsec.importdesc.index_unchecked(0uz)};
        imported_funcs.reserve(curr_module.imported_function_vec_storage.size());
        for(auto const& imported_func: curr_module.imported_function_vec_storage)
        {
            imported_funcs.push_back_unchecked(
                append_runtime_import.template operator()<validation_module_traits_t::external_types::func>(imported_func.import_type_ptr));
        }

        auto& imported_tables{importsec.importdesc.index_unchecked(1uz)};
        imported_tables.reserve(curr_module.imported_table_vec_storage.size());
        for(auto const& imported_table: curr_module.imported_table_vec_storage)
        {
            imported_tables.push_back_unchecked(
                append_runtime_import.template operator()<validation_module_traits_t::external_types::table>(imported_table.import_type_ptr));
        }

        auto& imported_memories{importsec.importdesc.index_unchecked(2uz)};
        imported_memories.reserve(curr_module.imported_memory_vec_storage.size());
        for(auto const& imported_memory: curr_module.imported_memory_vec_storage)
        {
            imported_memories.push_back_unchecked(
                append_runtime_import.template operator()<validation_module_traits_t::external_types::memory>(imported_memory.import_type_ptr));
        }

        auto& imported_globals{importsec.importdesc.index_unchecked(3uz)};
        imported_globals.reserve(curr_module.imported_global_vec_storage.size());
        for(auto const& imported_global: curr_module.imported_global_vec_storage)
        {
            imported_globals.push_back_unchecked(
                append_runtime_import.template operator()<validation_module_traits_t::external_types::global>(imported_global.import_type_ptr));
        }

        auto& imported_tags{importsec.importdesc.index_unchecked(4uz)};
        imported_tags.reserve(curr_module.imported_tag_vec_storage.size());
        for(auto const& imported_tag: curr_module.imported_tag_vec_storage)
        {
            // The import vector was reserved for ALL five descriptor kinds above. Appending tags cannot
            // invalidate pointers previously published in imported_funcs/tables/memories/globals.
            imported_tags.push_back_unchecked(
                append_runtime_import.template operator()<validation_module_traits_t::external_types::tag>(imported_tag.import_type_ptr));
        }
        tagsec.type_indices.reserve(curr_module.local_defined_tag_vec_storage.size());
        for(auto const& tag: curr_module.local_defined_tag_vec_storage)
        {
            if(tag.function_type_ptr == nullptr || tag.type_index >= type_count) [[unlikely]] { runtime_storage_bug(); }
            tagsec.type_indices.push_back_unchecked(tag.type_index);
        }

        funcsec.funcs.change_mode(::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u32_vector);
        funcsec.funcs.storage.typeidx_u32_vector.reserve(curr_module.local_defined_function_vec_storage.size());
        codesec.codes.reserve(curr_module.local_defined_function_vec_storage.size());
        for(auto const& local_func: curr_module.local_defined_function_vec_storage)
        {
            if(local_func.function_type_ptr == nullptr || local_func.wasm_code_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
            funcsec.funcs.storage.typeidx_u32_vector.push_back_unchecked(get_runtime_type_index(curr_module, local_func.function_type_ptr));
            codesec.codes.push_back_unchecked(*local_func.wasm_code_ptr);
        }

        tablesec.tables.reserve(curr_module.local_defined_table_vec_storage.size());
        for(auto const& local_table: curr_module.local_defined_table_vec_storage)
        {
            if(local_table.table_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
            tablesec.tables.push_back_unchecked(*local_table.table_type_ptr);
        }

        memsec.memories.reserve(curr_module.local_defined_memory_vec_storage.size());
        for(auto const& local_memory: curr_module.local_defined_memory_vec_storage)
        {
            if(local_memory.memory_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
            memsec.memories.push_back_unchecked(*local_memory.memory_type_ptr);
        }

        globalsec.local_globals.reserve(curr_module.local_defined_global_vec_storage.size());
        for(auto const& local_global: curr_module.local_defined_global_vec_storage)
        {
            if(local_global.local_global_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
            globalsec.local_globals.push_back_unchecked(*local_global.local_global_type_ptr);
        }

        // The standard validators derive the declared ref.func set from exports, globals, and element segments.
        // Runtime storage already keeps that exact union; synthetic function exports preserve export-only declarations.
        exportsec.exports.reserve(curr_module.declared_ref_funcidx_vec_storage.size());
        using synthetic_export_t = ::std::remove_cvref_t<decltype(exportsec.exports.front_unchecked())>;
        for(auto const func_index: curr_module.declared_ref_funcidx_vec_storage)
        {
            synthetic_export_t synthetic_export{};
            synthetic_export.exports.type = validation_module_traits_t::external_types::func;
            synthetic_export.exports.storage.func_idx = func_index;
            exportsec.exports.push_back_unchecked(::std::move(synthetic_export));
        }

        elemsec.elems.reserve(curr_module.local_defined_element_vec_storage.size());
        for(auto const& local_element: curr_module.local_defined_element_vec_storage)
        {
            if(local_element.element_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
            elemsec.elems.push_back_unchecked(*local_element.element_type_ptr);
        }

        datasec.datas.reserve(curr_module.local_defined_data_vec_storage.size());
        for(auto const& local_data: curr_module.local_defined_data_vec_storage)
        {
            if(local_data.data_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
            datasec.datas.push_back_unchecked(*local_data.data_type_ptr);
        }

        datacountsec.present = curr_module.data_count_section_present;
        datacountsec.count = curr_module.data_count_section_count;

        return validation_module;
    }
