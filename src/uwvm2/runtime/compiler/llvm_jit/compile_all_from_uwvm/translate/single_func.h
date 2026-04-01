struct local_func_storage_t
{
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* function_type_ptr{};
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_code_t const* wasm_code_ptr{};
    ::std::byte const* code_begin{};
    ::std::byte const* code_end{};
    ::std::size_t function_index{};
};

struct full_function_symbol_t
{
    ::uwvm2::utils::container::vector<local_func_storage_t> local_funcs{};
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

    [[nodiscard]] inline validation_module_storage_t
        build_runtime_validation_module(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module)
    {
        validate_runtime_module_storage(curr_module);

        validation_module_storage_t validation_module{};

        auto& importsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::import_section_storage_t>(
                validation_module.sections)};
        auto& typesec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::type_section_storage_t>(
                validation_module.sections)};
        auto& funcsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::function_section_storage_t>(
                validation_module.sections)};
        auto& codesec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::code_section_storage_t>(
                validation_module.sections)};
        auto& tablesec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::table_section_storage_t>(
                validation_module.sections)};
        auto& memsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::memory_section_storage_t>(
                validation_module.sections)};
        auto& globalsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::global_section_storage_t>(
                validation_module.sections)};

        {
            auto const type_count{get_runtime_type_section_count(curr_module)};
            typesec.types.reserve(type_count);

            auto const type_begin{curr_module.type_section_storage.type_section_begin};
            auto const type_end{curr_module.type_section_storage.type_section_end};

            if(type_count != 0uz)
            {
                for(auto curr_type{type_begin}; curr_type != type_end; ++curr_type)
                {
                    typesec.types.push_back_unchecked(*curr_type);
                }
            }
        }

        {
            auto const total_import_count{curr_module.imported_function_vec_storage.size() + curr_module.imported_table_vec_storage.size() +
                                          curr_module.imported_memory_vec_storage.size() + curr_module.imported_global_vec_storage.size()};
            importsec.imports.reserve(total_import_count);

            auto const append_runtime_import{
                [&]<validation_module_traits_t::external_types ExpectedKind>(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t const* import_type_ptr)
                    constexpr noexcept -> ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t const*
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

    inline constexpr void validate_runtime_local_func(validation_module_storage_t const& validation_module,
                                                      local_func_storage_t const& local_func_storage,
                                                      ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        ::uwvm2::validation::standard::wasm1::validate_code(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version{},
                                                            validation_module,
                                                            local_func_storage.function_index,
                                                            local_func_storage.code_begin,
                                                            local_func_storage.code_end,
                                                            err);
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
                .function_index = function_index};
    }

    inline constexpr local_func_storage_t compile_all_from_uwvm_local_func(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                           validation_module_storage_t const& validation_module,
                                                                           [[maybe_unused]] compile_option const& options,
                                                                           ::std::size_t local_function_idx,
                                                                           ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        local_func_storage_t local_func_storage{get_runtime_local_func_storage(curr_module, local_function_idx, err)};
        validate_runtime_local_func(validation_module, local_func_storage, err);
        return local_func_storage;
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
    // Phase 1 for llvm_jit: reuse the parser-storage validator verbatim before transplanting the runtime-storage validator.
    return ::uwvm2::uwvm::runtime::validator::validate_all_wasm_code_for_module(module_storage, file_name, module_name);
}

inline constexpr bool validate_all_wasm_code() noexcept
{
    // Phase 1 for llvm_jit: keep module-wide validation identical to the existing runtime validator.
    return ::uwvm2::uwvm::runtime::validator::validate_all_wasm_code();
}

inline constexpr void validate_runtime_wasm_code_for_module(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                            ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
{
    details::validate_runtime_module_all_local_funcs(curr_module, err);
}

[[nodiscard]] inline constexpr compile_task_split_config
    resolve_effective_compile_task_split_config([[maybe_unused]] ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                compile_task_split_config split_config,
                                                [[maybe_unused]] ::std::size_t extra_compile_threads) noexcept
{ return split_config; }

[[nodiscard]] inline constexpr ::std::size_t
    resolve_effective_adaptive_extra_compile_threads([[maybe_unused]] ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                     [[maybe_unused]] compile_task_split_config split_config,
                                                     [[maybe_unused]] ::std::size_t extra_compile_threads_upper_bound,
                                                     [[maybe_unused]] ::std::size_t target_task_groups_per_adjusted_compile_thread,
                                                     [[maybe_unused]] bool split_was_adjusted) noexcept
{ return 0uz; }

inline full_function_symbol_t compile_all_from_uwvm(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                    [[maybe_unused]] compile_option& options,
                                                    ::uwvm2::validation::error::code_validation_error_impl& err,
                                                    [[maybe_unused]] ::std::size_t extra_compile_threads,
                                                    compile_task_split_config split_config = {}) UWVM_THROWS
{
    full_function_symbol_t storage{};
    auto const validation_module{details::build_runtime_validation_module(curr_module)};

    split_config = resolve_effective_compile_task_split_config(curr_module, split_config, extra_compile_threads);
    static_cast<void>(split_config);

    auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
    storage.local_funcs.reserve(local_func_count);

    for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
    {
        storage.local_funcs.push_back(details::compile_all_from_uwvm_local_func(curr_module, validation_module, options, local_function_idx, err));
    }

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
    storage.local_funcs.push_back(details::compile_all_from_uwvm_local_func(curr_module, validation_module, options, 0uz, err));
    return storage;
}
