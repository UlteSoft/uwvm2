struct local_func_storage_t
{
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* function_type_ptr{};
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_code_t const* wasm_code_ptr{};
    ::std::byte const* code_begin{};
    ::std::byte const* code_end{};
    ::std::size_t module_id{};
    ::std::size_t function_index{};
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* runtime_module_ptr{};
};

struct llvm_jit_module_storage_t
{
    bool emitted{};
    ::std::unique_ptr<::llvm::LLVMContext> llvm_context_holder{};
    ::std::unique_ptr<::llvm::Module> llvm_module{};
};

struct full_function_symbol_t
{
    ::uwvm2::utils::container::vector<local_func_storage_t> local_funcs{};
    llvm_jit_module_storage_t llvm_jit_module{};
    ::std::size_t local_count{};
    ::std::size_t local_bytes_max{};
    ::std::size_t local_bytes_zeroinit_end{};
    ::std::size_t operand_stack_max{};
    ::std::size_t operand_stack_byte_max{};
};

struct compile_option
{
    ::std::size_t curr_wasm_id{};
};

enum class compile_task_split_policy_t : unsigned
{
    function_count,
    code_size
};

struct compile_task_split_config
{
    compile_task_split_policy_t policy{compile_task_split_policy_t::code_size};
    ::std::size_t split_size{4096uz};
    bool adjust_for_default_policy{true};
};

namespace details
{
    struct local_function_task_group
    {
        ::std::size_t begin_index{};
        ::std::size_t end_index{};
    };

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
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;
    };

    using validation_module_traits_t = validation_module_traits<::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_features_t>;
    using validation_module_storage_t = validation_module_traits_t::module_storage_t;

    using wasm1_code = ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic;
    using runtime_operand_stack_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
    using runtime_virtual_register_id = ::std::size_t;

    inline constexpr runtime_virtual_register_id invalid_runtime_virtual_register_id{::std::numeric_limits<runtime_virtual_register_id>::max()};

    struct runtime_operand_stack_storage_t
    {
        runtime_operand_stack_value_type type{};
        runtime_virtual_register_id virtual_register_id{invalid_runtime_virtual_register_id};
    };

    using runtime_operand_stack_type = ::uwvm2::utils::container::deque<runtime_operand_stack_storage_t>;

    struct runtime_local_virtual_register_t
    {
        runtime_operand_stack_value_type type{};
        runtime_virtual_register_id virtual_register_id{invalid_runtime_virtual_register_id};
    };

    using runtime_local_virtual_register_table_t = ::uwvm2::utils::container::deque<runtime_local_virtual_register_t>;

    struct runtime_block_result_type
    {
        runtime_operand_stack_value_type const* begin{};
        runtime_operand_stack_value_type const* end{};
    };

    enum class block_type : unsigned
    {
        function,
        block,
        loop,
        if_,
        else_
    };

    struct runtime_block_t
    {
        runtime_block_result_type result{};
        ::std::size_t operand_stack_base{};
        block_type type{};
        bool polymorphic_base{};
        bool then_polymorphic_end{};
    };

    [[noreturn]] inline void runtime_storage_bug() noexcept
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

    [[nodiscard]] inline ::std::size_t get_runtime_type_section_count(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module) noexcept
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

    [[nodiscard]] inline validation_module_traits_t::wasm_u32
        get_runtime_type_index(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                               ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* function_type_ptr) noexcept
    {
        using wasm_u32 = validation_module_traits_t::wasm_u32;

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

        auto const type_index_uz{byte_offset / sizeof(*type_begin)};
        if(type_index_uz > static_cast<::std::size_t>(::std::numeric_limits<wasm_u32>::max())) [[unlikely]] { runtime_storage_bug(); }

        return static_cast<wasm_u32>(type_index_uz);
    }

    inline constexpr void validate_runtime_module_storage(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module) noexcept
    {
        using external_types = validation_module_traits_t::external_types;

        auto const imported_func_count{curr_module.imported_function_vec_storage.size()};
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        auto const imported_table_count{curr_module.imported_table_vec_storage.size()};
        auto const local_table_count{curr_module.local_defined_table_vec_storage.size()};
        auto const imported_memory_count{curr_module.imported_memory_vec_storage.size()};
        auto const local_memory_count{curr_module.local_defined_memory_vec_storage.size()};
        auto const imported_global_count{curr_module.imported_global_vec_storage.size()};
        auto const local_global_count{curr_module.local_defined_global_vec_storage.size()};

        static_cast<void>(checked_cast_size_to_wasm_u32(imported_func_count));
        static_cast<void>(checked_cast_size_to_wasm_u32(local_func_count));
        static_cast<void>(checked_cast_size_to_wasm_u32(imported_table_count));
        static_cast<void>(checked_cast_size_to_wasm_u32(local_table_count));
        static_cast<void>(checked_cast_size_to_wasm_u32(imported_memory_count));
        static_cast<void>(checked_cast_size_to_wasm_u32(local_memory_count));
        static_cast<void>(checked_cast_size_to_wasm_u32(imported_global_count));
        static_cast<void>(checked_cast_size_to_wasm_u32(local_global_count));
        static_cast<void>(get_runtime_type_section_count(curr_module));

        for(auto const& imported_func: curr_module.imported_function_vec_storage)
        {
            auto const import_type_ptr{imported_func.import_type_ptr};
            if(import_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
            if(import_type_ptr->imports.type != external_types::func) [[unlikely]] { runtime_storage_bug(); }
            if(import_type_ptr->imports.storage.function == nullptr) [[unlikely]] { runtime_storage_bug(); }
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
            if(import_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
            if(import_type_ptr->imports.type != external_types::table) [[unlikely]] { runtime_storage_bug(); }
        }

        for(auto const& local_table: curr_module.local_defined_table_vec_storage)
        {
            if(local_table.table_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
        }

        for(auto const& imported_memory: curr_module.imported_memory_vec_storage)
        {
            auto const import_type_ptr{imported_memory.import_type_ptr};
            if(import_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
            if(import_type_ptr->imports.type != external_types::memory) [[unlikely]] { runtime_storage_bug(); }
        }

        for(auto const& local_memory: curr_module.local_defined_memory_vec_storage)
        {
            if(local_memory.memory_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
        }

        for(auto const& imported_global: curr_module.imported_global_vec_storage)
        {
            auto const import_type_ptr{imported_global.import_type_ptr};
            if(import_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
            if(import_type_ptr->imports.type != external_types::global) [[unlikely]] { runtime_storage_bug(); }
        }

        for(auto const& local_global: curr_module.local_defined_global_vec_storage)
        {
            if(local_global.global_type_ptr == nullptr || local_global.local_global_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }

            auto const& global_type{*local_global.global_type_ptr};
            auto const& local_global_type{local_global.local_global_type_ptr->global};

            if(global_type.type != local_global_type.type || global_type.is_mutable != local_global_type.is_mutable) [[unlikely]] { runtime_storage_bug(); }
        }
    }

    [[nodiscard]] inline ::std::size_t
        calculate_local_function_task_unit(::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const& local_func,
                                           ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t split_policy) noexcept
    {
        if(split_policy == ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t::function_count) { return 1uz; }

        auto const& body{local_func.wasm_code_ptr->body};
        auto const code_begin{reinterpret_cast<::std::byte const*>(body.code_begin)};
        auto const code_end{reinterpret_cast<::std::byte const*>(body.code_end)};
        return static_cast<::std::size_t>(code_end - code_begin);
    }

    [[nodiscard]] inline ::uwvm2::utils::container::vector<local_function_task_group>
        build_local_function_task_groups(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                         ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_config split_config)
    {
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

    [[nodiscard]] inline ::std::size_t calculate_total_local_function_task_weight(
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t split_policy) noexcept
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

    [[nodiscard]] inline ::std::size_t
        calculate_local_function_task_group_count(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                  ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_config split_config) noexcept
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

    [[nodiscard]] inline bool
        should_run_local_functions_serially(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_config split_config,
                                            ::std::size_t extra_compile_threads) noexcept
    {
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        if(extra_compile_threads == 0uz || local_func_count <= 1uz) { return true; }

        auto const split_size{split_config.split_size == 0uz ? 1uz : split_config.split_size};

        if(split_config.policy == ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t::function_count)
        {
            return local_func_count <= split_size;
        }

        auto const total_task_weight{calculate_total_local_function_task_weight(curr_module, split_config.policy)};
        return total_task_weight <= split_size;
    }

    [[nodiscard]] inline validation_module_storage_t build_runtime_validation_module(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module)
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

        {
            auto const type_count{get_runtime_type_section_count(curr_module)};
            typesec.types.reserve(type_count);

            auto const type_begin{curr_module.type_section_storage.type_section_begin};
            auto const type_end{curr_module.type_section_storage.type_section_end};

            if(type_count != 0uz)
            {
                for(auto curr_type{type_begin}; curr_type != type_end; ++curr_type) { typesec.types.push_back_unchecked(*curr_type); }
            }
        }

        {
            auto const total_import_count{curr_module.imported_function_vec_storage.size() + curr_module.imported_table_vec_storage.size() +
                                          curr_module.imported_memory_vec_storage.size() + curr_module.imported_global_vec_storage.size()};
            importsec.imports.reserve(total_import_count);

            auto const append_runtime_import{[&]<validation_module_traits_t::external_types ExpectedKind>(
                                                 ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t const* import_type_ptr) constexpr noexcept
                                                 -> ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t const*
                                             {
                                                 if(import_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
                                                 if(import_type_ptr->imports.type != ExpectedKind) [[unlikely]] { runtime_storage_bug(); }

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
        }

        funcsec.funcs.change_mode(::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u32_vector);

        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        funcsec.funcs.storage.typeidx_u32_vector.reserve(local_func_count);
        codesec.codes.reserve(local_func_count);

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

        return validation_module;
    }

#include "single_func_emit.h"

    inline constexpr void validate_runtime_local_func(validation_module_storage_t const& module_storage,
                                                      local_func_storage_t const& local_func_storage,
                                                      ::uwvm2::validation::error::code_validation_error_impl& err,
                                                      llvm_jit_module_storage_t* emitted_llvm_jit_ir_storage = nullptr) UWVM_THROWS
    {
        auto const function_index{local_func_storage.function_index};
        auto const code_begin{local_func_storage.code_begin};
        auto const code_end{local_func_storage.code_end};
        // check
        auto const& importsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::import_section_storage_t>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 0uz);
        auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};
        if(function_index < import_func_count) [[unlikely]]
        {
            err.err_curr = code_begin;
            err.err_selectable.not_local_function.function_index = function_index;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::not_local_function;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        auto const local_func_idx{function_index - import_func_count};

        auto const& funcsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>(
                module_storage.sections)};
        auto const local_func_count{funcsec.funcs.size()};
        if(local_func_idx >= local_func_count) [[unlikely]]
        {
            err.err_curr = code_begin;
            err.err_selectable.invalid_function_index.function_index = function_index;
            // this add will never overflow, because it has been validated in parsing.
            err.err_selectable.invalid_function_index.all_function_size = import_func_count + local_func_count;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_function_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        auto const& typesec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::type_section_storage_t>(module_storage.sections)};

        auto const& curr_func_type{typesec.types.index_unchecked(funcsec.funcs.index_unchecked(local_func_idx))};
        auto const func_parameter_begin{curr_func_type.parameter.begin};
        auto const func_parameter_end{curr_func_type.parameter.end};
        auto const func_parameter_count_uz{static_cast<::std::size_t>(func_parameter_end - func_parameter_begin)};
        auto const func_parameter_count_u32{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(func_parameter_count_uz)};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(func_parameter_count_u32 != func_parameter_count_uz) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

        auto const& codesec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::code_section_storage_t>(module_storage.sections)};

        auto const& curr_code{codesec.codes.index_unchecked(local_func_idx)};
        auto const& curr_code_locals{curr_code.locals};

        // all local count = parameter + local defined local count
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_local_count{func_parameter_count_u32};
        for(auto const& local_part: curr_code_locals)
        {
            // all_local_count never overflow and never exceed the max of size_t
            all_local_count += local_part.count;
        }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if constexpr(::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max() > ::std::numeric_limits<::std::size_t>::max())
        {
            if(all_local_count > ::std::numeric_limits<::std::size_t>::max()) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
        }
#endif

        auto const& globalsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::global_section_storage_t>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 3uz);
        auto const& imported_globals{importsec.importdesc.index_unchecked(3u)};
        auto const imported_global_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_globals.size())};
        auto const local_global_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(globalsec.local_globals.size())};
        // all_global_count never overflow and never exceed the max of u32 (validated by parser limits)
        auto const all_global_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_global_count + local_global_count)};

        // table
        auto const& tablesec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::table_section_storage_t>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 1uz);
        auto const& imported_tables{importsec.importdesc.index_unchecked(1u)};
        auto const imported_table_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_tables.size())};
        auto const local_table_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(tablesec.tables.size())};
        // all_table_count never overflow and never exceed the max of u32 (validated by parser limits)
        auto const all_table_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_table_count + local_table_count)};

        // memory
        auto const& memsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::memory_section_storage_t>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 2uz);
        auto const& imported_memories{importsec.importdesc.index_unchecked(2u)};
        auto const imported_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_memories.size())};
        auto const local_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(memsec.memories.size())};
        // all_memory_count never overflow and never exceed the max of u32 (validated by parser limits)
        auto const all_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_memory_count + local_memory_count)};

        // control-flow stack
        using curr_block_type = runtime_block_t;
        ::uwvm2::utils::container::vector<curr_block_type> control_flow_stack{};

        // Operand stack / virtual register machine.
        //
        // This pass still validates the function with WebAssembly's stack-machine semantics, but
        // it simultaneously assigns every live value a monotonic virtual-register id so the later
        // LLVM lowering step can work against a de-stackified register view.
        //
        // Register allocation policy:
        // - Wasm locals receive stable register ids eagerly at function entry.
        // - Every operand-stack push allocates a fresh transient register id.
        // - Popping a stack value ends the stack extent of that transient register.
        //
        // A deque is used for the operand stack because this structure is append/pop heavy and we
        // do not want growth to imply whole-buffer relocation while the translator is taking
        // control-flow snapshots and repeatedly truncating back to block boundaries.
        using curr_operand_stack_value_type = runtime_operand_stack_value_type;
        using curr_operand_stack_type = runtime_operand_stack_type;
        using curr_runtime_virtual_register_id = runtime_virtual_register_id;
        using curr_local_virtual_register_t = runtime_local_virtual_register_t;
        using curr_local_virtual_register_table_t = runtime_local_virtual_register_table_t;
        curr_operand_stack_type operand_stack{};
        curr_local_virtual_register_table_t local_virtual_registers{};
        bool is_polymorphic{};

        curr_runtime_virtual_register_id next_virtual_register_id{};

        for(::std::size_t i{}; i != func_parameter_count_uz; ++i)
        {
            local_virtual_registers.push_back(
                curr_local_virtual_register_t{.type = func_parameter_begin[i], .virtual_register_id = next_virtual_register_id++});
        }
        for(auto const& local_part: curr_code_locals)
        {
            for(validation_module_traits_t::wasm_u32 i{}; i != local_part.count; ++i)
            {
                local_virtual_registers.push_back(curr_local_virtual_register_t{.type = local_part.type, .virtual_register_id = next_virtual_register_id++});
            }
        }

        if(local_virtual_registers.size() != static_cast<::std::size_t>(all_local_count)) [[unlikely]] { runtime_storage_bug(); }

        auto const local_virtual_register_from_index{[&](validation_module_traits_t::wasm_u32 local_index) constexpr -> curr_local_virtual_register_t const&
                                                     {
                                                         auto const idx{static_cast<::std::size_t>(local_index)};
                                                         if(idx >= local_virtual_registers.size()) [[unlikely]] { runtime_storage_bug(); }
                                                         return local_virtual_registers[idx];
                                                     }};

        auto const local_type_from_index{[&](validation_module_traits_t::wasm_u32 local_index) constexpr noexcept -> curr_operand_stack_value_type
                                         { return local_virtual_register_from_index(local_index).type; }};

        auto const allocate_virtual_register{[&]() constexpr noexcept -> curr_runtime_virtual_register_id
                                             {
                                                 auto const virtual_register_id{next_virtual_register_id};
                                                 if(virtual_register_id == invalid_runtime_virtual_register_id) [[unlikely]] { runtime_storage_bug(); }
                                                 ++next_virtual_register_id;
                                                 if(next_virtual_register_id == invalid_runtime_virtual_register_id) [[unlikely]] { runtime_storage_bug(); }
                                                 return virtual_register_id;
                                             }};

        auto const operand_stack_push{
            [&](curr_operand_stack_value_type type) constexpr
            { operand_stack.push_back(runtime_operand_stack_storage_t{.type = type, .virtual_register_id = allocate_virtual_register()}); }};

        auto const operand_stack_pop_unchecked{[&]() constexpr -> runtime_operand_stack_storage_t
                                               {
                                                   if(operand_stack.empty()) [[unlikely]] { runtime_storage_bug(); }
                                                   auto const value{operand_stack.back()};
                                                   operand_stack.pop_back();
                                                   return value;
                                               }};

        auto const operand_stack_pop_n{[&](::std::size_t n) constexpr
                                       {
                                           while(n-- != 0uz && !operand_stack.empty()) { static_cast<void>(operand_stack_pop_unchecked()); }
                                       }};

        auto const operand_stack_truncate_to{[&](::std::size_t new_size) constexpr
                                             {
                                                 while(operand_stack.size() > new_size) { static_cast<void>(operand_stack_pop_unchecked()); }
                                             }};

        struct concrete_operand_t
        {
            bool from_stack{};
            curr_operand_stack_value_type type{};
        };

        using code_validation_error_code = ::uwvm2::validation::error::code_validation_error_code;

        auto const curr_frame_operand_stack_base{[&]() constexpr noexcept -> ::std::size_t
                                                 {
                                                     if(control_flow_stack.empty()) { return 0uz; }
                                                     return control_flow_stack.back_unchecked().operand_stack_base;
                                                 }};

        auto const concrete_operand_count{[&]() constexpr noexcept -> ::std::size_t
                                          {
                                              auto const base{curr_frame_operand_stack_base()};
                                              auto const stack_size{operand_stack.size()};
                                              return stack_size >= base ? (stack_size - base) : 0uz;
                                          }};

        auto const report_operand_stack_underflow{
            [&](::std::byte const* op_begin, ::uwvm2::utils::container::u8string_view op_name, ::std::size_t required_count) constexpr UWVM_THROWS
            {
                err.err_curr = op_begin;
                err.err_selectable.operand_stack_underflow.op_code_name = op_name;
                err.err_selectable.operand_stack_underflow.stack_size_actual = concrete_operand_count();
                err.err_selectable.operand_stack_underflow.stack_size_required = required_count;
                err.err_code = code_validation_error_code::operand_stack_underflow;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }};

        auto const try_pop_concrete_operand{[&]() constexpr -> concrete_operand_t
                                            {
                                                if(concrete_operand_count() == 0uz) { return {}; }
                                                auto const operand{operand_stack_pop_unchecked()};
                                                return {.from_stack = true, .type = operand.type};
                                            }};

        auto const try_peek_concrete_operand{[&]() constexpr -> concrete_operand_t
                                             {
                                                 if(concrete_operand_count() == 0uz) { return {}; }
                                                 return {.from_stack = true, .type = operand_stack.back().type};
                                             }};

        // block type
        using value_type_enum = curr_operand_stack_value_type;
        static constexpr value_type_enum i32_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32)};
        static constexpr value_type_enum i64_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64)};
        static constexpr value_type_enum f32_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32)};
        static constexpr value_type_enum f64_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64)};

        // function block (label/result type is the function result)
        control_flow_stack.push_back({
            .result = {.begin = curr_func_type.result.begin, .end = curr_func_type.result.end},
            .operand_stack_base = 0uz,
            .type = block_type::function,
            .polymorphic_base = false,
            .then_polymorphic_end = false
        });

        // start parse the code
        auto code_curr{code_begin};

        runtime_local_func_llvm_jit_emit_state_t llvm_jit_emit_state{};
        bool emit_llvm_jit_active{
            emitted_llvm_jit_ir_storage != nullptr && try_prepare_runtime_local_func_llvm_jit_emit_state(local_func_storage, *emitted_llvm_jit_ir_storage, llvm_jit_emit_state)};

        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;

        auto const validate_numeric_unary{[&](::uwvm2::utils::container::u8string_view op_name,
                                              curr_operand_stack_value_type expected_operand_type,
                                              curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
                                          {
                                              // op_name ...
                                              // [safe] unsafe (could be the section_end)
                                              // ^^ code_curr

                                              auto const op_begin{code_curr};

                                              // op_name ...
                                              // [safe] unsafe (could be the section_end)
                                              // ^^ op_begin

                                              ++code_curr;

                                              // op_name ...
                                              // [safe]  unsafe (could be the section_end)
                                              //         ^^ code_curr

                                              if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]]
                                              {
                                                  report_operand_stack_underflow(op_begin, op_name, 1uz);
                                              }

                                              auto const operand{try_pop_concrete_operand()};

                                              if(operand.from_stack && operand.type != expected_operand_type) [[unlikely]]
                                              {
                                                  err.err_curr = op_begin;
                                                  err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                                                  err.err_selectable.numeric_operand_type_mismatch.expected_type =
                                                      static_cast<wasm_value_type>(expected_operand_type);
                                                  err.err_selectable.numeric_operand_type_mismatch.actual_type = static_cast<wasm_value_type>(operand.type);
                                                  err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                                  ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                              }

                                              operand_stack_push(result_type);
                                          }};

        auto const validate_numeric_binary{
            [&](::uwvm2::utils::container::u8string_view op_name,
                curr_operand_stack_value_type expected_operand_type,
                curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
            {
                // op_name ...
                // [safe] unsafe (could be the section_end)
                // ^^ code_curr

                auto const op_begin{code_curr};

                // op_name ...
                // [safe] unsafe (could be the section_end)
                // ^^ op_begin

                ++code_curr;

                // op_name ...
                // [safe ] unsafe (could be the section_end)
                //         ^^ code_curr

                if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]] { report_operand_stack_underflow(op_begin, op_name, 2uz); }

                auto const rhs{try_pop_concrete_operand()};
                if(rhs.from_stack && rhs.type != expected_operand_type) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = static_cast<wasm_value_type>(expected_operand_type);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = static_cast<wasm_value_type>(rhs.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                auto const lhs{try_pop_concrete_operand()};
                if(lhs.from_stack && lhs.type != expected_operand_type) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = static_cast<wasm_value_type>(expected_operand_type);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = static_cast<wasm_value_type>(lhs.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                operand_stack_push(result_type);
            }};

        auto const validate_mem_load{[&](::uwvm2::utils::container::u8string_view op_name,
                                         ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const max_align,
                                         curr_operand_stack_value_type const result_type) constexpr UWVM_THROWS
                                     {
                                         auto const op_begin{code_curr};
                                         ++code_curr;

                                         ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                                         ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                                         using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                         auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                                     reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                     ::fast_io::mnp::leb128_get(align))};
                                         if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                                         {
                                             err.err_curr = op_begin;
                                             err.err_code = code_validation_error_code::invalid_memarg_align;
                                             ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                                         }

                                         code_curr = reinterpret_cast<::std::byte const*>(align_next);

                                         auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                       ::fast_io::mnp::leb128_get(offset))};
                                         if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                                         {
                                             err.err_curr = op_begin;
                                             err.err_code = code_validation_error_code::invalid_memarg_offset;
                                             ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                                         }

                                         code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                                         if(all_memory_count == 0u) [[unlikely]]
                                         {
                                             err.err_curr = op_begin;
                                             err.err_selectable.no_memory.op_code_name = op_name;
                                             err.err_selectable.no_memory.align = align;
                                             err.err_selectable.no_memory.offset = offset;
                                             err.err_code = code_validation_error_code::no_memory;
                                             ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                         }

                                         if(align > max_align) [[unlikely]]
                                         {
                                             err.err_curr = op_begin;
                                             err.err_selectable.illegal_memarg_alignment.op_code_name = op_name;
                                             err.err_selectable.illegal_memarg_alignment.align = align;
                                             err.err_selectable.illegal_memarg_alignment.max_align = max_align;
                                             err.err_code = code_validation_error_code::illegal_memarg_alignment;
                                             ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                         }

                                         if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]]
                                         {
                                             report_operand_stack_underflow(op_begin, op_name, 1uz);
                                         }

                                         if(auto const addr{try_pop_concrete_operand()}; addr.from_stack &&
                                                                                         addr.type != curr_operand_stack_value_type::i32)
                                             [[unlikely]]
                                         {
                                             err.err_curr = op_begin;
                                             err.err_selectable.memarg_address_type_not_i32.op_code_name = op_name;
                                             err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                                             err.err_code = code_validation_error_code::memarg_address_type_not_i32;
                                             ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                         }

                                         operand_stack_push(result_type);
                                     }};

        auto const validate_mem_store{
            [&](::uwvm2::utils::container::u8string_view op_name,
                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const max_align,
                curr_operand_stack_value_type const expected_value_type) constexpr UWVM_THROWS
            {
                auto const op_begin{code_curr};
                ++code_curr;

                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                auto const [align_next, align_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(align))};
                if(align_err != ::fast_io::parse_code::ok) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_code = code_validation_error_code::invalid_memarg_align;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(align_err);
                }

                code_curr = reinterpret_cast<::std::byte const*>(align_next);

                auto const [offset_next, offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                              ::fast_io::mnp::leb128_get(offset))};
                if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_code = code_validation_error_code::invalid_memarg_offset;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                }

                code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                if(all_memory_count == 0u) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.no_memory.op_code_name = op_name;
                    err.err_selectable.no_memory.align = align;
                    err.err_selectable.no_memory.offset = offset;
                    err.err_code = code_validation_error_code::no_memory;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                if(align > max_align) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.illegal_memarg_alignment.op_code_name = op_name;
                    err.err_selectable.illegal_memarg_alignment.align = align;
                    err.err_selectable.illegal_memarg_alignment.max_align = max_align;
                    err.err_code = code_validation_error_code::illegal_memarg_alignment;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]]
                {
                    report_operand_stack_underflow(op_begin, op_name, 2uz);
                }

                auto const value{try_pop_concrete_operand()};
                auto const addr{try_pop_concrete_operand()};

                if(addr.from_stack && addr.type != curr_operand_stack_value_type::i32) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.memarg_address_type_not_i32.op_code_name = op_name;
                    err.err_selectable.memarg_address_type_not_i32.addr_type = addr.type;
                    err.err_code = code_validation_error_code::memarg_address_type_not_i32;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                if(value.from_stack && value.type != expected_value_type) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.store_value_type_mismatch.op_code_name = op_name;
                    err.err_selectable.store_value_type_mismatch.expected_type = static_cast<wasm_value_type>(expected_value_type);
                    err.err_selectable.store_value_type_mismatch.actual_type = value.type;
                    err.err_code = code_validation_error_code::store_value_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }};

#include "single_func_validation_dispatch.h"
    }

    inline constexpr local_func_storage_t get_runtime_local_func_storage(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                         ::std::size_t local_function_idx,
                                                                         ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        auto const import_func_count{curr_module.imported_function_vec_storage.size()};
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        if(local_function_idx >= local_func_count) [[unlikely]]
        {
            err.err_curr = nullptr;
            err.err_selectable.invalid_function_index.function_index = import_func_count + local_function_idx;
            err.err_selectable.invalid_function_index.all_function_size = import_func_count + local_func_count;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_function_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        auto const function_index{import_func_count + local_function_idx};
        auto const& curr_local_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_idx)};
        if(curr_local_func.function_type_ptr == nullptr || curr_local_func.wasm_code_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }

        auto const code_begin{reinterpret_cast<::std::byte const*>(curr_local_func.wasm_code_ptr->body.expr_begin)};
        auto const code_end{reinterpret_cast<::std::byte const*>(curr_local_func.wasm_code_ptr->body.code_end)};
        if(code_begin == nullptr || code_end == nullptr || code_begin > code_end) [[unlikely]] { runtime_storage_bug(); }

        return {.function_type_ptr = curr_local_func.function_type_ptr,
                .wasm_code_ptr = curr_local_func.wasm_code_ptr,
                .code_begin = code_begin,
                .code_end = code_end,
                .function_index = function_index,
                .runtime_module_ptr = ::std::addressof(curr_module)};
    }

    inline constexpr local_func_storage_t compile_all_from_uwvm_local_func(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                           validation_module_storage_t const& validation_module,
                                                                           [[maybe_unused]] compile_option const& options,
                                                                           ::std::size_t local_function_idx,
                                                                           ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                           llvm_jit_module_storage_t* emitted_llvm_jit_ir_storage = nullptr) UWVM_THROWS
    {
        local_func_storage_t local_func_storage{get_runtime_local_func_storage(curr_module, local_function_idx, err)};
        local_func_storage.module_id = options.curr_wasm_id;
        validate_runtime_local_func(validation_module, local_func_storage, err, emitted_llvm_jit_ir_storage);
        return local_func_storage;
    }

    inline void compile_all_from_uwvm_local_func_group(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                       validation_module_storage_t const& validation_module,
                                                       [[maybe_unused]] compile_option const& options,
                                                       full_function_symbol_t& storage,
                                                       local_function_task_group task_group,
                                                       ::uwvm2::validation::error::code_validation_error_impl& err,
                                                       llvm_jit_module_storage_t* emitted_llvm_jit_ir_storage = nullptr) UWVM_THROWS
    {
        for(::std::size_t local_function_idx{task_group.begin_index}; local_function_idx != task_group.end_index; ++local_function_idx)
        {
            storage.local_funcs.index_unchecked(local_function_idx) = compile_all_from_uwvm_local_func(curr_module,
                                                                                                       validation_module,
                                                                                                       options,
                                                                                                       local_function_idx,
                                                                                                       err,
                                                                                                       emitted_llvm_jit_ir_storage);
        }
    }

    struct parallel_compile_failure_state
    {
        ::std::atomic_bool failed{};
        ::std::atomic_flag failure_claim = ATOMIC_FLAG_INIT;
        ::uwvm2::validation::error::code_validation_error_impl err{};
#ifdef UWVM_CPP_EXCEPTIONS
        ::std::exception_ptr exception{};
        bool has_err{};
#endif
    };

#ifdef UWVM_CPP_EXCEPTIONS
    inline void publish_parallel_compile_failure(parallel_compile_failure_state& failure_state,
                                                 ::uwvm2::validation::error::code_validation_error_impl const& local_err,
                                                 ::std::exception_ptr exception,
                                                 bool store_err) noexcept
    {
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

    inline ::uwvm2::utils::thread::scheduled_task
        make_compile_all_from_uwvm_local_func_group_task(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                         validation_module_storage_t const& validation_module,
                                                         [[maybe_unused]] compile_option const& options,
                                                         full_function_symbol_t& storage,
                                                         parallel_compile_failure_state& failure_state,
                                                         local_function_task_group task_group,
                                                         llvm_jit_module_storage_t* emitted_llvm_jit_ir_storage = nullptr)
    {
#ifdef UWVM_CPP_EXCEPTIONS
        if(failure_state.failed.load(::std::memory_order_acquire)) { co_return; }
#endif

        ::uwvm2::validation::error::code_validation_error_impl local_err{};

#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            compile_all_from_uwvm_local_func_group(
                curr_module, validation_module, options, storage, task_group, local_err, emitted_llvm_jit_ir_storage);
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

    [[nodiscard]] inline bool link_llvm_jit_module_fragments(llvm_jit_module_storage_t& merged_module_storage,
                                                             ::uwvm2::utils::container::vector<llvm_jit_module_storage_t>& task_module_storages)
    {
        if(merged_module_storage.llvm_context_holder == nullptr || merged_module_storage.llvm_module == nullptr) [[unlikely]] { return false; }

        auto& merged_llvm_context{*merged_module_storage.llvm_context_holder};
        ::llvm::Linker linker(*merged_module_storage.llvm_module);
        for(auto& task_module_storage: task_module_storages)
        {
            if(task_module_storage.llvm_context_holder == nullptr || task_module_storage.llvm_module == nullptr) [[unlikely]] { return false; }

            ::uwvm2::utils::container::string serialized_bitcode{};
            {
                // Reparse each fragment into the merged context before linking. Direct
                // cross-context linking hit unstable intrinsic remangling on this toolchain.
                raw_uwvm_string_ostream bitcode_stream(serialized_bitcode);
                ::llvm::WriteBitcodeToFile(*task_module_storage.llvm_module, bitcode_stream);
            }

            auto parsed_task_module_expected{
                ::llvm::parseBitcodeFile(::llvm::MemoryBufferRef(get_llvm_string_ref(serialized_bitcode), task_module_storage.llvm_module->getName()),
                                         merged_llvm_context)};
            if(!parsed_task_module_expected) [[unlikely]]
            {
                ::llvm::consumeError(parsed_task_module_expected.takeError());
                return false;
            }

            if(linker.linkInModule(::std::move(*parsed_task_module_expected))) [[unlikely]] { return false; }
            task_module_storage.llvm_module.reset();
            task_module_storage.llvm_context_holder.reset();
            task_module_storage.emitted = false;
        }

        return true;
    }

    inline constexpr void validate_runtime_module_all_local_funcs(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                  ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        auto const validation_module{build_runtime_validation_module(curr_module)};
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};

        for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
        {
            auto const local_func_storage{get_runtime_local_func_storage(curr_module, local_function_idx, err)};
            validate_runtime_local_func(validation_module, local_func_storage, err);
        }
    }
}  // namespace details

inline constexpr ::std::size_t default_small_module_code_size_threshold{512uz * 1024uz};
inline constexpr ::std::size_t default_target_task_groups_per_compile_thread{4uz};
inline constexpr ::std::size_t default_target_task_groups_per_adjusted_compile_thread{4uz};
inline constexpr ::std::size_t aggressive_target_task_groups_per_adjusted_compile_thread{5uz};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
inline bool validate_all_wasm_code_for_module(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
                                              ::uwvm2::utils::container::u8cstring_view file_name,
                                              ::uwvm2::utils::container::u8string_view module_name) noexcept
{
    details::validation_module_storage_t const& validation_module{module_storage};

    auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<details::validation_module_traits_t::import_section_storage_t>(
        validation_module.sections)};
    auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};

    auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<details::validation_module_traits_t::code_section_storage_t>(
        validation_module.sections)};

    for(::std::size_t local_idx{}; local_idx < codesec.codes.size(); ++local_idx)
    {
        auto const& code{codesec.codes.index_unchecked(local_idx)};
        auto const code_begin_ptr{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
        auto const code_end_ptr{reinterpret_cast<::std::byte const*>(code.body.code_end)};

        ::uwvm2::validation::error::code_validation_error_impl v_err{};
#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            details::validate_runtime_local_func(validation_module,
                                                 {.code_begin = code_begin_ptr, .code_end = code_end_ptr, .function_index = import_func_count + local_idx},
                                                 v_err);
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
            ::uwvm2::uwvm::utils::memory::print_memory const memory_printer{validation_module.module_span.module_begin,
                                                                            v_err.err_curr,
                                                                            validation_module.module_span.module_end};

            ::uwvm2::validation::error::error_output_t errout{};
            errout.module_begin = validation_module.module_span.module_begin;
            errout.err = v_err;
            errout.flag.enable_ansi = static_cast<::std::uint_least8_t>(::uwvm2::uwvm::utils::ansies::put_color);
# if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
            errout.flag.win32_use_text_attr = static_cast<::std::uint_least8_t>(!::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b);
# endif

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Validation error in WebAssembly Code (module=\"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                module_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\", file=\"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                file_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\").\n",
                                errout,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\n" u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Validator Memory Indication: ",
                                memory_printer,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                u8"\n\n");

            return false;
        }
#endif
    }

    return true;
}

inline constexpr bool validate_all_wasm_code() noexcept
{
    ::fast_io::unix_timestamp start_time{};
    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
    {
        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                            u8"[info]  ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"Start validating all wasm code. ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                            u8"[",
                            ::uwvm2::uwvm::io::get_local_realtime(),
                            u8"] ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                            u8"(verbose)\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            start_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
        }
#endif
    }

    for(auto const& [module_name, mod]: ::uwvm2::uwvm::wasm::storage::all_module)
    {
        switch(mod.type)
        {
            case ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm: [[fallthrough]];
            case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm:
            {
                auto const wf{mod.module_storage_ptr.wf};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(wf == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

                switch(wf->binfmt_ver)
                {
                    case 1u:
                    {
                        if(!validate_all_wasm_code_for_module(wf->wasm_module_storage.wasm_binfmt_ver1_storage, wf->file_name, module_name)) { return false; }
                        break;
                    }
                    [[unlikely]] default:
                    {
                        static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
                        break;
                    }
                }
                break;
            }
            case ::uwvm2::uwvm::wasm::type::module_type_t::local_import:
            {
                break;
            }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
            case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_dl:
            {
                break;
            }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
            case ::uwvm2::uwvm::wasm::type::module_type_t::weak_symbol:
            {
                break;
            }
#endif
            [[unlikely]] default:
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::std::unreachable();
            }
        }
    }

    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
    {
        ::fast_io::unix_timestamp end_time{};

#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            end_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
        }
#endif

        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                            u8"[info]  ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"Validate all wasm code done. (time=",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                            end_time - start_time,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"s). ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                            u8"[",
                            ::uwvm2::uwvm::io::get_local_realtime(),
                            u8"] ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                            u8"(verbose)\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
    }

    return true;
}

inline constexpr void validate_runtime_wasm_code_for_module(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                            ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
{ details::validate_runtime_module_all_local_funcs(curr_module, err); }

[[nodiscard]] inline constexpr compile_task_split_config
    resolve_effective_compile_task_split_config(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                compile_task_split_config split_config,
                                                ::std::size_t extra_compile_threads) noexcept
{
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

[[nodiscard]] inline ::std::size_t resolve_effective_adaptive_extra_compile_threads(
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
    compile_task_split_config split_config,
    ::std::size_t extra_compile_threads_upper_bound,
    ::std::size_t target_task_groups_per_adjusted_compile_thread,
    bool split_was_adjusted) noexcept
{
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

inline full_function_symbol_t compile_all_from_uwvm(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                    [[maybe_unused]] compile_option& options,
                                                    ::uwvm2::validation::error::code_validation_error_impl& err,
                                                    ::std::size_t extra_compile_threads,
                                                    compile_task_split_config split_config = {}) UWVM_THROWS
{
    full_function_symbol_t storage{};
    auto const validation_module{details::build_runtime_validation_module(curr_module)};

    split_config = resolve_effective_compile_task_split_config(curr_module, split_config, extra_compile_threads);

    auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
    storage.local_funcs.clear();
    storage.local_funcs.resize(local_func_count);

    auto const compile_local_functions_serially{
        [&]() UWVM_THROWS
        {
            auto const emit_llvm_jit_active{details::try_prepare_runtime_llvm_jit_module_storage(curr_module, storage.llvm_jit_module)};
            for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
            {
                storage.local_funcs.index_unchecked(local_function_idx) = details::compile_all_from_uwvm_local_func(curr_module,
                                                                                                                     validation_module,
                                                                                                                     options,
                                                                                                                     local_function_idx,
                                                                                                                     err,
                                                                                                                     emit_llvm_jit_active
                                                                                                                         ? ::std::addressof(
                                                                                                                               storage.llvm_jit_module)
                                                                                                                         : nullptr);
            }
            storage.llvm_jit_module.emitted = storage.llvm_jit_module.llvm_context_holder != nullptr && storage.llvm_jit_module.llvm_module != nullptr;
        }};

    if(details::should_run_local_functions_serially(curr_module, split_config, extra_compile_threads))
    {
        compile_local_functions_serially();
        return storage;
    }

    auto const task_groups{details::build_local_function_task_groups(curr_module, split_config)};
    auto const effective_extra_compile_threads{::uwvm2::utils::thread::clamp_extra_worker_count(task_groups.size(), extra_compile_threads)};

    if(effective_extra_compile_threads == 0uz)
    {
        compile_local_functions_serially();
        return storage;
    }

    if(!details::try_prepare_runtime_llvm_jit_module_storage(curr_module, storage.llvm_jit_module))
    {
        compile_local_functions_serially();
        return storage;
    }

    ::uwvm2::utils::container::vector<llvm_jit_module_storage_t> task_llvm_jit_modules{};
    task_llvm_jit_modules.resize(task_groups.size());

    bool prepared_task_llvm_jit_modules{true};
    for(auto& task_llvm_jit_module: task_llvm_jit_modules)
    {
        if(!details::try_prepare_runtime_llvm_jit_module_storage(curr_module, task_llvm_jit_module))
        {
            prepared_task_llvm_jit_modules = false;
            break;
        }
    }

    if(!prepared_task_llvm_jit_modules)
    {
        compile_local_functions_serially();
        return storage;
    }

    ::uwvm2::utils::thread::scheduled_task_batch task_batch{task_groups.size()};

    details::parallel_compile_failure_state failure_state{};
    for(::std::size_t task_group_index{}; task_group_index != task_groups.size(); ++task_group_index)
    {
        auto task{details::make_compile_all_from_uwvm_local_func_group_task(curr_module,
                                                                            validation_module,
                                                                            options,
                                                                            storage,
                                                                            failure_state,
                                                                            task_groups.index_unchecked(task_group_index),
                                                                            ::std::addressof(task_llvm_jit_modules.index_unchecked(task_group_index)))};
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

    if(!details::link_llvm_jit_module_fragments(storage.llvm_jit_module, task_llvm_jit_modules))
    {
        compile_local_functions_serially();
        return storage;
    }

    storage.llvm_jit_module.emitted = storage.llvm_jit_module.llvm_context_holder != nullptr && storage.llvm_jit_module.llvm_module != nullptr;

    return storage;
}

inline full_function_symbol_t compile_all_from_uwvm_single_func(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                [[maybe_unused]] compile_option& options,
                                                                ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
{
    full_function_symbol_t storage{};
    if(curr_module.local_defined_function_vec_storage.empty()) { return storage; }
    auto const validation_module{details::build_runtime_validation_module(curr_module)};

    storage.local_funcs.reserve(1uz);
    auto const emit_llvm_jit_active{details::try_prepare_runtime_llvm_jit_module_storage(curr_module, storage.llvm_jit_module)};
    storage.local_funcs.push_back(details::compile_all_from_uwvm_local_func(curr_module,
                                                                            validation_module,
                                                                            options,
                                                                            0uz,
                                                                            err,
                                                                            emit_llvm_jit_active ? ::std::addressof(storage.llvm_jit_module) : nullptr));
    storage.llvm_jit_module.emitted = storage.llvm_jit_module.llvm_context_holder != nullptr && storage.llvm_jit_module.llvm_module != nullptr;
    return storage;
}
