enum class compile_task_split_policy_t : unsigned
{
    // Split by function count for predictable task cardinality, or by code size to balance large
    // modules where a few functions dominate translation time.
    function_count,
    code_size
};

struct compile_task_split_config
{
    // `split_size` is interpreted according to `policy`; the default uses bytecode size because it
    // correlates better with validation/emission work than raw function count.
    compile_task_split_policy_t policy{compile_task_split_policy_t::code_size};
    ::std::size_t split_size{4096uz};
    bool adjust_for_default_policy{true};
};

namespace details
{
    using full_function_symbol_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t;
    using parser_feature_parameter_t = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t;

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
        using element_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>;
        using data_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::data_section_storage_t<Fs...>;
        using data_count_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>;
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

        static_cast<void>(checked_cast_size_to_wasm_u32(curr_module.imported_function_vec_storage.size()));
        static_cast<void>(checked_cast_size_to_wasm_u32(curr_module.local_defined_function_vec_storage.size()));
        static_cast<void>(checked_cast_size_to_wasm_u32(curr_module.imported_table_vec_storage.size()));
        static_cast<void>(checked_cast_size_to_wasm_u32(curr_module.local_defined_table_vec_storage.size()));
        static_cast<void>(checked_cast_size_to_wasm_u32(curr_module.imported_memory_vec_storage.size()));
        static_cast<void>(checked_cast_size_to_wasm_u32(curr_module.local_defined_memory_vec_storage.size()));
        static_cast<void>(checked_cast_size_to_wasm_u32(curr_module.imported_global_vec_storage.size()));
        static_cast<void>(checked_cast_size_to_wasm_u32(curr_module.local_defined_global_vec_storage.size()));
        static_cast<void>(get_runtime_type_section_count(curr_module));

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
    }

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
        auto& elemsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::element_section_storage_t>(
            validation_module.sections)};
        auto& datasec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::data_section_storage_t>(
            validation_module.sections)};
        auto& datacountsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::data_count_section_storage_t>(
            validation_module.sections)};

        auto const type_count{get_runtime_type_section_count(curr_module)};
        typesec.types.reserve(type_count);
        for(auto type_curr{curr_module.type_section_storage.type_section_begin}; type_curr != curr_module.type_section_storage.type_section_end; ++type_curr)
        {
            typesec.types.push_back_unchecked(*type_curr);
        }

        auto const total_import_count{curr_module.imported_function_vec_storage.size() + curr_module.imported_table_vec_storage.size() +
                                      curr_module.imported_memory_vec_storage.size() + curr_module.imported_global_vec_storage.size()};
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

        if(!curr_module.local_defined_data_vec_storage.empty())
        {
            datacountsec.present = true;
            datacountsec.count = checked_cast_size_to_wasm_u32(curr_module.local_defined_data_vec_storage.size());
        }

        return validation_module;
    }

    inline constexpr void validate_runtime_module_with_standard_wasm1p1_validator(
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
        ::uwvm2::validation::error::code_validation_error_impl& err,
        parser_feature_parameter_t const* wasm_feature_parameter) UWVM_THROWS
    {
        parser_feature_parameter_t const default_wasm_feature_parameter{};
        auto const& effective_wasm_feature_parameter{wasm_feature_parameter == nullptr ? default_wasm_feature_parameter : *wasm_feature_parameter};
        auto const validation_module{build_runtime_validation_module(curr_module)};
        auto const import_func_count{curr_module.imported_function_vec_storage.size()};

        for(::std::size_t local_function_idx{}; local_function_idx != curr_module.local_defined_function_vec_storage.size(); ++local_function_idx)
        {
            auto const& local_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_idx)};
            if(local_func.wasm_code_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
            auto const code_begin{reinterpret_cast<::std::byte const*>(local_func.wasm_code_ptr->body.expr_begin)};
            auto const code_end{reinterpret_cast<::std::byte const*>(local_func.wasm_code_ptr->body.code_end)};
            ::uwvm2::validation::standard::wasm1p1::validate_code(::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
                                                                  validation_module,
                                                                  import_func_count + local_function_idx,
                                                                  code_begin,
                                                                  code_end,
                                                                  err,
                                                                  effective_wasm_feature_parameter);
        }
    }

    struct local_function_task_group
    {
        ::std::size_t begin_index{};
        ::std::size_t end_index{};
    };

    inline constexpr void initialize_local_defined_call_info(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                             ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option const& options,
                                                             full_function_symbol_t& storage) noexcept
    {
        // Pre-size call-info beside local function storage so direct-call metadata can later be
        // filled in by local-defined index without reallocating during or after parallel compilation.
        auto const import_func_count{curr_module.imported_function_vec_storage.size()};
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};

        storage.local_funcs.clear();
        storage.local_funcs.resize(local_func_count);
        storage.local_defined_call_info.clear();
        storage.local_defined_call_info.resize(local_func_count);

        for(::std::size_t i{}; i != local_func_count; ++i)
        {
            auto& info{storage.local_defined_call_info.index_unchecked(i)};
            info.module_id = options.curr_wasm_id;
            info.function_index = import_func_count + i;
        }
    }

    [[nodiscard]] inline constexpr ::std::size_t
        calculate_local_function_task_unit(::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const& local_func,
                                           ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_policy_t split_policy) noexcept
    {
        // Code-size weighting gives the scheduler a cheap approximation of translation cost while
        // keeping the policy switch deterministic and allocation-free.
        if(split_policy == ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_policy_t::function_count) { return 1uz; }

        auto const& body{local_func.wasm_code_ptr->body};
        auto const code_begin{reinterpret_cast<::std::byte const*>(body.code_begin)};
        auto const code_end{reinterpret_cast<::std::byte const*>(body.code_end)};
        return static_cast<::std::size_t>(code_end - code_begin);
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<local_function_task_group>
        build_local_function_task_groups(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                         ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_config split_config)
    {
        // Groups are contiguous so each worker writes a stable slice of `storage.local_funcs` and
        // `storage.local_defined_call_info`, avoiding synchronization around per-function outputs.
        ::uwvm2::utils::container::vector<local_function_task_group> task_groups{};

        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        if(local_func_count == 0uz) { return task_groups; }

        task_groups.reserve(local_func_count);

        auto const split_size{split_config.split_size == 0uz ? 1uz : split_config.split_size};

        ::std::size_t group_begin_index{};
        ::std::size_t current_group_weight{};

        for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
        {
            auto const task_unit{
                calculate_local_function_task_unit(curr_module.local_defined_function_vec_storage.index_unchecked(local_function_idx), split_config.policy)};

            if(task_unit > (::std::numeric_limits<::std::size_t>::max() - current_group_weight)) [[unlikely]]
            {
                current_group_weight = ::std::numeric_limits<::std::size_t>::max();
            }
            else
            {
                current_group_weight += task_unit;
            }

            if(current_group_weight >= split_size)
            {
                task_groups.push_back_unchecked({.begin_index = group_begin_index, .end_index = local_function_idx + 1uz});
                group_begin_index = local_function_idx + 1uz;
                current_group_weight = 0uz;
            }
        }

        if(group_begin_index != local_func_count) { task_groups.push_back_unchecked({.begin_index = group_begin_index, .end_index = local_func_count}); }

        return task_groups;
    }

    [[nodiscard]] inline constexpr ::std::size_t calculate_total_local_function_task_weight(
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
        ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_policy_t split_policy) noexcept
    {
        ::std::size_t total_weight{};

        for(auto const& local_func: curr_module.local_defined_function_vec_storage)
        {
            auto const task_unit{calculate_local_function_task_unit(local_func, split_policy)};
            if(task_unit > (::std::numeric_limits<::std::size_t>::max() - total_weight)) [[unlikely]] { return ::std::numeric_limits<::std::size_t>::max(); }
            total_weight += task_unit;
        }

        return total_weight;
    }

    [[nodiscard]] inline constexpr ::std::size_t
        calculate_local_function_task_group_count(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                  ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_config split_config) noexcept
    {
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        if(local_func_count == 0uz) { return 0uz; }

        auto const split_size{split_config.split_size == 0uz ? 1uz : split_config.split_size};

        ::std::size_t task_group_count{};
        ::std::size_t current_group_weight{};

        for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
        {
            auto const task_unit{
                calculate_local_function_task_unit(curr_module.local_defined_function_vec_storage.index_unchecked(local_function_idx), split_config.policy)};

            if(task_unit > (::std::numeric_limits<::std::size_t>::max() - current_group_weight)) [[unlikely]]
            {
                current_group_weight = ::std::numeric_limits<::std::size_t>::max();
            }
            else
            {
                current_group_weight += task_unit;
            }

            if(current_group_weight >= split_size)
            {
                ++task_group_count;
                current_group_weight = 0uz;
            }
        }

        if(current_group_weight != 0uz) { ++task_group_count; }
        return task_group_count;
    }

    [[nodiscard]] inline constexpr bool
        should_run_local_functions_serially(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                            ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_config split_config,
                                            ::std::size_t extra_compile_threads) noexcept
    {
        // Avoid thread-pool overhead when the module is tiny or the split policy would produce only
        // one meaningful task group.
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        if(extra_compile_threads == 0uz || local_func_count <= 1uz) { return true; }

        auto const split_size{split_config.split_size == 0uz ? 1uz : split_config.split_size};

        if(split_config.policy == ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_policy_t::function_count)
        {
            return local_func_count <= split_size;
        }

        auto const total_task_weight{calculate_total_local_function_task_weight(curr_module, split_config.policy)};
        return total_task_weight <= split_size;
    }

    inline constexpr void aggregate_local_function_storage(full_function_symbol_t& storage) noexcept
    {
        // The runtime allocates frame storage from module-level maxima, so aggregate each translated
        // function's requirements after all local bodies have been compiled.
        storage.local_count = 0uz;
        storage.local_bytes_max = 0uz;
        storage.local_bytes_zeroinit_end = 0uz;
        storage.operand_stack_max = 0uz;
        storage.operand_stack_byte_max = 0uz;

        for(auto const& local_func: storage.local_funcs)
        {
            storage.local_count = ::std::max(storage.local_count, local_func.local_count);
            storage.local_bytes_max = ::std::max(storage.local_bytes_max, local_func.local_bytes_max);
            storage.local_bytes_zeroinit_end = ::std::max(storage.local_bytes_zeroinit_end, local_func.local_bytes_zeroinit_end);
            storage.operand_stack_max = ::std::max(storage.operand_stack_max, local_func.operand_stack_max);
            storage.operand_stack_byte_max = ::std::max(storage.operand_stack_byte_max, local_func.operand_stack_byte_max);
        }
    }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    inline constexpr void compile_all_from_uwvm_local_func(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                           [[maybe_unused]] ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                                                           full_function_symbol_t& storage,
                                                           ::std::size_t compile_local_function_idx,
                                                           parser_feature_parameter_t const* wasm_feature_parameter,
                                                           ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS{
// These include fragments are intentionally expanded inside the function body: they share a large
// translation context by lexical scope while keeping context setup, emit helpers, and dispatch logic
// in separate reviewable files.
#define UWVM_COMPILE_SINGLE_LOCAL_FUNCTION 1
#include "single_func_context.h"
#include "single_func_emit_helpers.h"
#include "single_func_validation_dispatch.h"
#undef UWVM_COMPILE_SINGLE_LOCAL_FUNCTION
    }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    inline constexpr void compile_all_from_uwvm_local_func_group(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                 [[maybe_unused]] ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                                                                 full_function_symbol_t& storage,
                                                                 local_function_task_group task_group,
                                                                 parser_feature_parameter_t const* wasm_feature_parameter,
                                                                 ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        for(::std::size_t local_function_idx{task_group.begin_index}; local_function_idx != task_group.end_index; ++local_function_idx)
        {
            compile_all_from_uwvm_local_func<CompileOption>(curr_module, options, storage, local_function_idx, wasm_feature_parameter, err);
        }
    }

    struct parallel_compile_failure_state
    {
        // Parallel compilation keeps the first failure because that is the most useful diagnostic and
        // avoids racing writes to the shared validation error object.
        ::std::atomic_bool failed{};
        ::std::atomic_flag failure_claim = ATOMIC_FLAG_INIT;
        ::uwvm2::validation::error::code_validation_error_impl err{};
#ifdef UWVM_CPP_EXCEPTIONS
        ::std::exception_ptr exception{};
        bool has_err{};
#endif
    };

#ifdef UWVM_CPP_EXCEPTIONS
    inline constexpr void publish_parallel_compile_failure(parallel_compile_failure_state& failure_state,
                                                           ::uwvm2::validation::error::code_validation_error_impl const& local_err,
                                                           ::std::exception_ptr exception,
                                                           bool store_err) noexcept
    {
        // Only the first worker that claims the flag publishes diagnostic detail; later failures only
        // set `failed` so the caller can stop trusting partial results.
        if(!failure_state.failure_claim.test_and_set(::std::memory_order_acq_rel))
        {
            if(store_err)
            {
                failure_state.err = local_err;
                failure_state.has_err = true;
            }
            failure_state.exception = ::std::move(exception);
        }
        failure_state.failed.store(true, ::std::memory_order_release);
    }
#endif

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    inline ::uwvm2::utils::thread::scheduled_task
        make_compile_all_from_uwvm_local_func_group_task(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                         ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                                                         full_function_symbol_t& storage,
                                                         parallel_compile_failure_state& failure_state,
                                                         parser_feature_parameter_t const* wasm_feature_parameter,
                                                         local_function_task_group task_group)
    {
        // Each scheduled task compiles a contiguous group and reports failure through shared state;
        // the coroutine wrapper lets the existing thread scheduler own task lifetime uniformly.
#ifdef UWVM_CPP_EXCEPTIONS
        if(failure_state.failed.load(::std::memory_order_acquire)) { co_return; }
#endif

        ::uwvm2::validation::error::code_validation_error_impl local_err{};

#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            compile_all_from_uwvm_local_func_group<CompileOption>(curr_module, options, storage, task_group, wasm_feature_parameter, local_err);
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error const&)
        {
            publish_parallel_compile_failure(failure_state, local_err, ::std::current_exception(), true);
        }
        catch(...)
        {
            publish_parallel_compile_failure(failure_state, local_err, ::std::current_exception(), false);
        }
#endif

        co_return;
    }
}  // namespace details

inline constexpr ::std::size_t default_small_module_code_size_threshold{512uz * 1024uz};
inline constexpr ::std::size_t default_target_task_groups_per_compile_thread{4uz};
inline constexpr ::std::size_t default_target_task_groups_per_adjusted_compile_thread{4uz};
inline constexpr ::std::size_t aggressive_target_task_groups_per_adjusted_compile_thread{5uz};

[[nodiscard]] inline constexpr compile_task_split_config
    resolve_effective_compile_task_split_config(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                compile_task_split_config split_config,
                                                ::std::size_t extra_compile_threads) noexcept
{
    // Small modules can suffer from oversplitting: adapt the default code-size split upward so there
    // are enough tasks for load balancing but not so many that scheduling dominates compilation.
    if(extra_compile_threads == 0uz || !split_config.adjust_for_default_policy || split_config.policy != compile_task_split_policy_t::code_size)
    {
        return split_config;
    }

    auto const total_code_size{details::calculate_total_local_function_task_weight(curr_module, split_config.policy)};
    if(total_code_size <= split_config.split_size || total_code_size > default_small_module_code_size_threshold) { return split_config; }

    auto const total_compile_threads{extra_compile_threads + 1uz};
    auto const target_task_group_count{total_compile_threads * default_target_task_groups_per_compile_thread};
    if(target_task_group_count == 0uz) [[unlikely]] { return split_config; }

    auto const adaptive_split_size{total_code_size / target_task_group_count + static_cast<::std::size_t>(total_code_size % target_task_group_count != 0uz)};

    if(adaptive_split_size > split_config.split_size) { split_config.split_size = adaptive_split_size; }
    return split_config;
}

[[nodiscard]] inline constexpr ::std::size_t
    resolve_effective_adaptive_extra_compile_threads(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                     compile_task_split_config split_config,
                                                     ::std::size_t extra_compile_threads_upper_bound,
                                                     ::std::size_t target_task_groups_per_adjusted_compile_thread,
                                                     bool split_was_adjusted) noexcept
{
    // After split-size adjustment, reduce requested worker count when the resulting task count cannot
    // keep all workers busy. This avoids paying for idle helper threads.
    auto const useful_extra_compile_threads{
        ::uwvm2::utils::thread::clamp_extra_worker_count(details::calculate_local_function_task_group_count(curr_module, split_config),
                                                         extra_compile_threads_upper_bound)};
    if(useful_extra_compile_threads == 0uz || !split_was_adjusted || target_task_groups_per_adjusted_compile_thread == 0uz)
    {
        return useful_extra_compile_threads;
    }

    auto const task_group_count{details::calculate_local_function_task_group_count(curr_module, split_config)};
    if(task_group_count <= 1uz) { return 0uz; }

    auto const adjusted_total_compile_threads{task_group_count / target_task_groups_per_adjusted_compile_thread +
                                              static_cast<::std::size_t>(task_group_count % target_task_groups_per_adjusted_compile_thread != 0uz)};
    auto const adjusted_extra_compile_threads{adjusted_total_compile_threads > 1uz ? adjusted_total_compile_threads - 1uz : 0uz};
    return adjusted_extra_compile_threads < useful_extra_compile_threads ? adjusted_extra_compile_threads : useful_extra_compile_threads;
}

template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
inline constexpr ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t
    compile_all_from_uwvm(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                          [[maybe_unused]] ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                          ::uwvm2::validation::error::code_validation_error_impl& err,
                          ::std::size_t extra_compile_threads,
                          compile_task_split_config split_config = {},
                          details::parser_feature_parameter_t const* wasm_feature_parameter = nullptr) UWVM_THROWS
{
    // Module compilation happens in two phases: translate all local bodies first, then complete
    // call-info metadata once every compiled function record has a stable address.
    ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t storage{};

    auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
    details::initialize_local_defined_call_info(curr_module, options, storage);
    details::validate_runtime_module_with_standard_wasm1p1_validator(curr_module, err, wasm_feature_parameter);

    split_config = resolve_effective_compile_task_split_config(curr_module, split_config, extra_compile_threads);

    if(details::should_run_local_functions_serially(curr_module, split_config, extra_compile_threads))
    {
        for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
        {
            details::compile_all_from_uwvm_local_func<CompileOption>(curr_module, options, storage, local_function_idx, wasm_feature_parameter, err);
        }
        details::aggregate_local_function_storage(storage);

        // Call-info needs final local function storage and aggregated frame limits, so it is included
        // only after the serial translation path has finished all bodies.
        using wasm_value_type = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_value_type_t;

#include "single_func_call_info.h"

        return storage;
    }

    // Parallel mode first builds immutable task groups, then each worker translates its assigned
    // slice into pre-sized storage vectors.
    auto const task_groups{details::build_local_function_task_groups(curr_module, split_config)};
    auto const effective_extra_compile_threads{::uwvm2::utils::thread::clamp_extra_worker_count(task_groups.size(), extra_compile_threads)};

    if(effective_extra_compile_threads == 0uz)
    {
        for(auto const& task_group: task_groups)
        {
            details::compile_all_from_uwvm_local_func_group<CompileOption>(curr_module, options, storage, task_group, wasm_feature_parameter, err);
        }
    }
    else
    {
        ::uwvm2::utils::thread::scheduled_task_batch task_batch{task_groups.size()};

        details::parallel_compile_failure_state failure_state{};
        for(auto const& task_group: task_groups)
        {
            auto task{details::make_compile_all_from_uwvm_local_func_group_task<CompileOption>(
                curr_module,
                options,
                storage,
                failure_state,
                wasm_feature_parameter,
                task_group)};
            ::std::construct_at(task_batch.handles.buffer + task_batch.handle_count, task.release());
            ++task_batch.handle_count;
        }

        ::uwvm2::utils::thread::native_thread_pool thread_pool{};
        thread_pool.run(task_batch, effective_extra_compile_threads);

#ifdef UWVM_CPP_EXCEPTIONS
        if(failure_state.failed.load(::std::memory_order_acquire))
        {
            if(failure_state.has_err) { err = failure_state.err; }
            if(failure_state.exception) { ::std::rethrow_exception(failure_state.exception); }
            ::fast_io::fast_terminate();
        }
#endif
    }

    // Aggregation and call-info completion remain single-threaded so published module metadata is
    // derived from fully materialized local function records.
    details::aggregate_local_function_storage(storage);

    using wasm_value_type = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_value_type_t;

#include "single_func_call_info.h"

    return storage;
}

template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
inline constexpr ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t
    compile_all_from_uwvm_single_func(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                      [[maybe_unused]] ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                                      ::uwvm2::validation::error::code_validation_error_impl& err,
                                      details::parser_feature_parameter_t const* wasm_feature_parameter = nullptr) UWVM_THROWS
{ return compile_all_from_uwvm<CompileOption>(curr_module, options, err, 0uz, {}, wasm_feature_parameter); }
