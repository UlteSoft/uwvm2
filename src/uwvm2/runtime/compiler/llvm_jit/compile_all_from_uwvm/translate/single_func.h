// This header coordinates validation and optional LLVM JIT emission for local functions stored in the uwvm runtime module
// representation.  The code is header-only because the validator includes opcode-family case files and the LLVM emitter
// uses many local helper lambdas; keeping them in one translation context avoids a wide state-passing interface.
//
// The validator currently targets WebAssembly 1.0/MVP scalar semantics.  Places that intentionally rely on MVP limits are
// called out with "Wasm 1.0/MVP" comments so future proposal work (multi-value, reference-types, memory64, extended
// opcode spaces, etc.) has visible migration points.

// Metadata for one tiered/OSR loop entry that can re-enter a compiled function at a validated loop boundary.
struct tiered_loop_reentry_storage_t
{
    // Function-relative byte offset of the Wasm instruction that starts the reentry target.
    ::std::size_t wasm_code_offset{};

    // Non-zero id passed to the tiered core dispatcher.  Entry id zero is reserved for normal function entry.
    ::std::uint_least32_t entry_id{};
};

// Borrowed runtime storage needed to validate and optionally emit one local defined function.
struct local_func_storage_t
{
    // Finalized Wasm function type.  Borrowed from runtime module storage.
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* function_type_ptr{};

    // Finalized Wasm code body.  Borrowed from runtime module storage.
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_code_t const* wasm_code_ptr{};

    // Half-open byte range of the expression body.
    ::std::byte const* code_begin{};
    ::std::byte const* code_end{};

    // Runtime module id used by logical call-stack/unwind diagnostics.
    ::std::size_t module_id{};

    // Public module function index, including imported functions before local defined functions.
    ::std::size_t function_index{};

    // Owning runtime module.  Borrowed; generated code may embed this address for raw bridge calls.
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* runtime_module_ptr{};

    // OSR entries discovered while validating/emitting this function.
    ::uwvm2::utils::container::vector<tiered_loop_reentry_storage_t> tiered_loop_reentries{};
};

// LLVM objects owned by one emitted module fragment or by the final merged module.
struct llvm_jit_module_storage_t
{
    // True after finalization succeeds and the module/context pair is ready for optimization or execution.
    bool emitted{};

    // LLVM context must outlive the module, DIBuilder, functions, and all LLVM types/values owned by the module.
    ::uwvm2::utils::container::delete_owned_ptr<::llvm::LLVMContext> llvm_context_holder{};

    // LLVM IR module for a full compile or a per-task fragment.
    ::uwvm2::utils::container::delete_owned_ptr<::llvm::Module> llvm_module{};

    // Optional debug-info builder used for trap/unwind frame reconstruction.
    ::uwvm2::utils::container::delete_owned_ptr<::llvm::DIBuilder> llvm_di_builder{};

    // Debug metadata nodes are owned by `llvm_module`; these are borrowed convenience handles.
    ::llvm::DIFile* llvm_di_file{};
    ::llvm::DICompileUnit* llvm_di_compile_unit{};

    inline constexpr llvm_jit_module_storage_t() noexcept = default;
    inline constexpr llvm_jit_module_storage_t(llvm_jit_module_storage_t const&) noexcept = delete;
    inline constexpr llvm_jit_module_storage_t& operator= (llvm_jit_module_storage_t const&) noexcept = delete;
    inline constexpr llvm_jit_module_storage_t(llvm_jit_module_storage_t&&) noexcept = default;

    inline constexpr llvm_jit_module_storage_t& operator= (llvm_jit_module_storage_t&& other) noexcept
    {
        if(this == ::std::addressof(other)) [[unlikely]] { return *this; }

        // Destroy dependent LLVM objects before replacing the context.  LLVM IR objects keep context-owned uniqued types
        // and metadata, so resetting in dependency order avoids dangling ownership during move assignment.
        llvm_module.reset();
        llvm_di_builder.reset();
        llvm_context_holder.reset();

        emitted = other.emitted;
        llvm_context_holder = ::std::move(other.llvm_context_holder);
        llvm_module = ::std::move(other.llvm_module);
        llvm_di_builder = ::std::move(other.llvm_di_builder);
        llvm_di_file = other.llvm_di_file;
        llvm_di_compile_unit = other.llvm_di_compile_unit;
        other.emitted = false;
        other.llvm_di_file = nullptr;
        other.llvm_di_compile_unit = nullptr;
        return *this;
    }
};

// Optional callback run on each task-local LLVM module before fragments are linked.
using llvm_jit_task_module_pre_link_callback_t = bool (*)(llvm_jit_module_storage_t&, void*) noexcept;

// Result container for compiling all local functions in a runtime module.
struct full_function_symbol_t
{
    // Per-local-function metadata in local function order.
    ::uwvm2::utils::container::vector<local_func_storage_t> local_funcs{};

    // Final merged LLVM module, or the serially emitted module when no parallel split is used.
    llvm_jit_module_storage_t llvm_jit_module{};

    // True when the pre-link callback ran successfully on task fragments before linking.
    bool llvm_jit_task_modules_pre_link_optimized{};

    // Reserved aggregate metrics for downstream runtime/JIT bookkeeping.
    ::std::size_t local_count{};
    ::std::size_t local_bytes_max{};
    ::std::size_t local_bytes_zeroinit_end{};
    ::std::size_t operand_stack_max{};
    ::std::size_t operand_stack_byte_max{};
};

inline constexpr bool default_verify_llvm_jit_ir{true};

// User/runtime options controlling validation-time LLVM JIT emission.
struct compile_option
{
    // Runtime module id used by generated diagnostics.
    ::std::size_t curr_wasm_id{};

    // Parser feature switches used by the built-in wasm1p1 validator gates. Null means the default disabled feature set.
    ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t const* validator_feature_parameter{};

    // Enables LLVM verifier checks after function/module finalization.
    bool verify_llvm_jit_ir{default_verify_llvm_jit_ir};

    // Forces Wasm calls through the runtime raw ABI instead of direct typed LLVM declarations.
    bool route_wasm_calls_through_runtime_bridge{};

    // Lazy target tables for raw and typed calls to locally defined functions.
    ::std::uintptr_t lazy_defined_raw_call_target_base_address{};
    ::std::size_t lazy_defined_raw_call_target_count{};
    ::std::uintptr_t lazy_defined_typed_entry_target_base_address{};
    ::std::size_t lazy_defined_typed_entry_target_count{};

    // True when lazy target table entries are concurrently published and must be loaded with acquire ordering.
    bool lazy_defined_targets_are_atomic{};

    // Enables generation of tiered loop reentry wrappers and hidden-core dispatch.
    bool emit_tiered_loop_reentry_entries{};

    // Emits logical Wasm call-stack push/pop around public entries.
    bool emit_call_stack_frames{true};

    // Emits compact DWARF/unwind metadata for optimized trap-stack reconstruction.
    bool emit_unwind_call_stack_frames{};

    // Optional per-task module callback used by optimization/linking pipelines.
    llvm_jit_task_module_pre_link_callback_t llvm_jit_task_module_pre_link_callback{};
    void* llvm_jit_task_module_pre_link_callback_context{};
};

// Unit used to split a module into parallel compilation tasks.
enum class compile_task_split_policy_t : unsigned
{
    // Split by number of local functions.
    function_count,

    // Split by byte size of Wasm function bodies.
    code_size
};

// Configuration for parallel task grouping.
struct compile_task_split_config
{
    // How task weight is computed.
    compile_task_split_policy_t policy{compile_task_split_policy_t::code_size};

    // Target task weight; zero is normalized to one.
    ::std::size_t split_size{4096uz};

    // Allow the default code-size policy to coarsen tiny modules to avoid over-splitting.
    bool adjust_for_default_policy{true};
};

namespace details
{
    // Half-open range of local-defined functions assigned to one compile task.
    struct local_function_task_group
    {
        ::std::size_t begin_index{};
        ::std::size_t end_index{};
    };

    // Compile-time adapter from the configured Wasm feature tuple to the parser section-storage types used by validation.
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
    using parser_feature_parameter_t = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t;

    // MVP primary opcode enum.  Future prefixed proposal opcodes must not be squeezed into this one-byte dispatch model;
    // extend the dispatch layer when those instructions are enabled.
    using wasm1_code = ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic;
    using wasm1p1_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_basic;
    using wasm1p1_numeric_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_numeric;

    // Finalized scalar operand type used by both the validator and LLVM JIT operand stack.
    using runtime_operand_stack_value_type = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_value_type_t;
    using runtime_diagnostic_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;

    [[nodiscard]] inline constexpr runtime_diagnostic_value_type to_wasm1_diagnostic_value_type(runtime_operand_stack_value_type type) noexcept
    { return static_cast<runtime_diagnostic_value_type>(type); }

    // Virtual register ids give the stack validator a de-stackified view for later JIT bookkeeping.
    using runtime_virtual_register_id = ::std::size_t;

    inline constexpr runtime_virtual_register_id invalid_runtime_virtual_register_id{::std::numeric_limits<runtime_virtual_register_id>::max()};

    // One validated value currently on the conceptual Wasm operand stack.
    struct runtime_operand_stack_storage_t
    {
        // MVP scalar type.
        runtime_operand_stack_value_type type{};

        // Monotonic id assigned when the value is produced.
        runtime_virtual_register_id virtual_register_id{invalid_runtime_virtual_register_id};
    };

    using runtime_operand_stack_type = ::uwvm2::utils::container::deque<runtime_operand_stack_storage_t>;

    // Stable virtual-register assignment for one Wasm local slot.
    struct runtime_local_virtual_register_t
    {
        runtime_operand_stack_value_type type{};
        runtime_virtual_register_id virtual_register_id{invalid_runtime_virtual_register_id};
    };

    using runtime_local_virtual_register_table_t = ::uwvm2::utils::container::deque<runtime_local_virtual_register_t>;

    // Runtime block result represented as a pointer pair into parser/runtime type storage or static MVP result arrays.
    // WebAssembly 1.0/MVP blocktypes are empty or single-value; this shape can represent more values, but the current JIT
    // emitter still documents and rejects multi-value lowering points until the multi-value proposal is enabled end to end.
    struct runtime_block_result_type
    {
        runtime_operand_stack_value_type const* begin{};
        runtime_operand_stack_value_type const* end{};
    };

    // Structured-control frame kind used by validation.
    enum class block_type : unsigned
    {
        function,
        block,
        loop,
        if_,
        else_
    };

    // Validation-time frame for one structured-control construct.
    struct runtime_block_t
    {
        // Result values required at the construct's merge/return point.
        runtime_block_result_type result{};

        // Operand stack height before entering the construct.
        ::std::size_t operand_stack_base{};

        // Construct kind.
        block_type type{};

        // True when the construct was entered from an unreachable/polymorphic region.
        bool polymorphic_base{};

        // True when an if-then arm ended polymorphically before else.
        bool then_polymorphic_end{};
    };

    // Runtime module storage should already be finalized by the parser/initializer.  Violations here mean host/runtime
    // corruption, not guest validation failure, so the JIT fails fast instead of reporting a Wasm error.
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
        // WebAssembly 1.0/MVP indices are u32.  If a future extension changes an index space width, the validation module
        // adapter and every u32 diagnostic field around this path must be audited together.
        if(sz > static_cast<::std::size_t>(::std::numeric_limits<wasm_u32>::max())) [[unlikely]] { runtime_storage_bug(); }
        return static_cast<wasm_u32>(sz);
    }

    // Count finalized function types from the runtime type-section pointer pair with defensive storage checks.
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
        // Pointer subtraction through integer addresses is intentional here: we are validating runtime storage layout,
        // including alignment to the finalized type object size.
        if(byte_span % type_size != 0uz) [[unlikely]] { runtime_storage_bug(); }
        return byte_span / type_size;
    }

    // Convert a finalized function-type pointer back into its type-section index.
    [[nodiscard]] inline constexpr validation_module_traits_t::wasm_u32
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
        // The pointer must refer to an actual type-section element, not just to an address inside the type-section span.
        if(byte_offset % sizeof(*type_begin) != 0uz) [[unlikely]] { runtime_storage_bug(); }

        auto const type_index_uz{byte_offset / sizeof(*type_begin)};
        if(type_index_uz > static_cast<::std::size_t>(::std::numeric_limits<wasm_u32>::max())) [[unlikely]] { runtime_storage_bug(); }

        return static_cast<wasm_u32>(type_index_uz);
    }

    inline constexpr void validate_runtime_module_storage(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module) noexcept
    {
        using external_types = validation_module_traits_t::external_types;

        // Validate every runtime index space against the MVP u32 limit before rebuilding a parser-style validation module.
        // The parser normally enforces these limits; this pass repeats the check because runtime storage may be assembled
        // from imports and finalized links after parsing.
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

        // Import records are copied into a parser-style module below.  Verify their external kind before copying so the
        // importdesc buckets stay type-homogeneous.
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
            // Local function code ranges are borrowed by validation and optional JIT emission.  Null or reversed ranges
            // would make opcode dispatch read outside the finalized function body.
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

            // The runtime keeps both a finalized global type and the original local-global declaration.  They must agree
            // before validation emits direct global accesses.
            if(global_type.type != local_global_type.type || global_type.is_mutable != local_global_type.is_mutable) [[unlikely]] { runtime_storage_bug(); }
        }
    }

    // Weight one local function for compile-task splitting.
    [[nodiscard]] inline constexpr ::std::size_t
        calculate_local_function_task_unit(::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const& local_func,
                                           ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t split_policy) noexcept
    {
        // Function-count mode treats every local function equally; code-size mode approximates compile cost with body
        // byte length, which is cheap to compute from finalized storage.
        if(split_policy == ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t::function_count) { return 1uz; }

        auto const& body{local_func.wasm_code_ptr->body};
        auto const code_begin{reinterpret_cast<::std::byte const*>(body.code_begin)};
        auto const code_end{reinterpret_cast<::std::byte const*>(body.code_end)};
        return static_cast<::std::size_t>(code_end - code_begin);
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<local_function_task_group>
        build_local_function_task_groups(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                         ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_config split_config) noexcept
    {
        ::uwvm2::utils::container::vector<local_function_task_group> task_groups{};

        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        if(local_func_count == 0uz) { return task_groups; }

        task_groups.reserve(local_func_count);

        // A zero split size would otherwise create empty groups or an infinite loop.  Normalize to one task unit.
        auto const split_size{split_config.split_size == 0uz ? 1uz : split_config.split_size};

        ::std::size_t group_begin_index{};
        ::std::size_t current_group_weight{};

        for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
        {
            auto const task_unit{
                calculate_local_function_task_unit(curr_module.local_defined_function_vec_storage.index_unchecked(local_function_idx), split_config.policy)};

            // Saturate group weight on overflow.  Once saturated, the next threshold check will close the group.
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
                // Groups are half-open local-function index ranges.  The current function belongs to the group that just
                // reached the threshold.
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
        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t split_policy) noexcept
    {
        ::std::size_t total_weight{};

        for(auto const& local_func: curr_module.local_defined_function_vec_storage)
        {
            auto const task_unit{calculate_local_function_task_unit(local_func, split_policy)};
            // Saturating the total preserves ordering decisions without risking undefined overflow.
            if(task_unit > (::std::numeric_limits<::std::size_t>::max() - total_weight)) [[unlikely]] { return ::std::numeric_limits<::std::size_t>::max(); }
            total_weight += task_unit;
        }

        return total_weight;
    }

    [[nodiscard]] inline constexpr ::std::size_t
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

    [[nodiscard]] inline constexpr bool
        should_run_local_functions_serially(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_config split_config,
                                            ::std::size_t extra_compile_threads) noexcept
    {
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        if(extra_compile_threads == 0uz || local_func_count <= 1uz) { return true; }

        // This mirrors task-group construction without allocating the group vector, so the caller can avoid parallel setup
        // when it would produce only one useful task.
        auto const split_size{split_config.split_size == 0uz ? 1uz : split_config.split_size};

        if(split_config.policy == ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t::function_count)
        {
            return local_func_count <= split_size;
        }

        auto const total_task_weight{calculate_total_local_function_task_weight(curr_module, split_config.policy)};
        return total_task_weight <= split_size;
    }

    [[nodiscard]] inline constexpr validation_module_storage_t
        build_runtime_validation_module(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module) noexcept
    {
        validate_runtime_module_storage(curr_module);

        // Rebuild a parser-style validation module from finalized runtime storage.  Opcode validation then reuses the same
        // section layout expected by normal parser validation while still validating the runtime module currently being
        // compiled.
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

        {
            auto const type_count{get_runtime_type_section_count(curr_module)};
            typesec.types.reserve(type_count);

            auto const type_begin{curr_module.type_section_storage.type_section_begin};
            auto const type_end{curr_module.type_section_storage.type_section_end};

            // Type objects are copied by value so the synthetic validation module has contiguous type storage of its own.
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
                                                 // Import descriptors are grouped by kind in wasm1 storage.  Copying
                                                 // through this helper keeps the descriptor vectors in the same order as
                                                 // the runtime import vectors while normalizing function type pointers.
                                                 if(import_type_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
                                                 if(import_type_ptr->imports.type != ExpectedKind) [[unlikely]] { runtime_storage_bug(); }

                                                 importsec.imports.push_back_unchecked(*import_type_ptr);
                                                 auto& copied_import{importsec.imports.back_unchecked()};

                                                 if constexpr(ExpectedKind == validation_module_traits_t::external_types::func)
                                                 {
                                                     // Parser validation expects function imports to point into the
                                                     // validation module's copied type section, not into runtime storage.
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

        // WebAssembly 1.0/MVP function/type indices are u32.  If index-width support changes in a future proposal, this
        // forced u32 function-section storage mode must be revisited with the parser storage policy.
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

#include "single_func_emit.h"

    // Validate one local function body and, when requested, emit inline LLVM IR for the same validated instruction stream.
    // Validation is authoritative: LLVM emission may be disabled at any point without stopping validation.
    inline constexpr void
        validate_runtime_local_func(validation_module_storage_t const& module_storage,
                                    local_func_storage_t const& local_func_storage,
                                    ::uwvm2::validation::error::code_validation_error_impl& err,
                                    llvm_jit_module_storage_t* emitted_llvm_jit_ir_storage = nullptr,
                                    bool verify_llvm_jit_ir = default_verify_llvm_jit_ir,
                                    bool route_wasm_calls_through_runtime_bridge = false,
                                    ::std::uintptr_t lazy_defined_raw_call_target_base_address = 0u,
                                    ::std::size_t lazy_defined_raw_call_target_count = 0uz,
                                    ::std::uintptr_t lazy_defined_typed_entry_target_base_address = 0u,
                                    ::std::size_t lazy_defined_typed_entry_target_count = 0uz,
                                    bool lazy_defined_targets_are_atomic = false,
                                    bool emit_tiered_loop_reentry_entries = false,
                                    bool emit_call_stack_frames = true,
                                    bool emit_unwind_call_stack_frames = false,
                                    parser_feature_parameter_t const* validator_feature_parameter = nullptr,
                                    ::uwvm2::utils::container::vector<tiered_loop_reentry_storage_t>* tiered_loop_reentries_out = nullptr) UWVM_THROWS
    {
        auto const function_index{local_func_storage.function_index};
        auto const code_begin{local_func_storage.code_begin};
        auto const code_end{local_func_storage.code_end};

        // Module function indices include imports first.  This helper validates local defined functions only; imported
        // functions have no Wasm body to validate or emit.
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

        // Map the public module function index back into the local function/code section index.
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

        // WebAssembly 1.0/MVP parameter indices are u32.  Multi-value does not change parameter index width, but any
        // future widening of Wasm index spaces must audit this cast and the diagnostics that store these counts.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(func_parameter_count_u32 != func_parameter_count_uz) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

        auto const& codesec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::code_section_storage_t>(module_storage.sections)};

        auto const& curr_code{codesec.codes.index_unchecked(local_func_idx)};
        auto const& curr_code_locals{curr_code.locals};

        // All local count = function parameters followed by locally declared locals.  Wasm local indices address this
        // flattened space.
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_local_count{func_parameter_count_u32};
        for(auto const& local_part: curr_code_locals)
        {
            // Parser validation has already enforced the MVP u32 local-index limit, so this addition is expected not to
            // overflow.  Keep the assumption visible because future index-width work must update this accounting.
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
        // MVP global indices are u32; imported and local globals share one index space.
        auto const all_global_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_global_count + local_global_count)};

        // table
        auto const& tablesec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::table_section_storage_t>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 1uz);
        auto const& imported_tables{importsec.importdesc.index_unchecked(1u)};
        auto const imported_table_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_tables.size())};
        auto const local_table_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(tablesec.tables.size())};
        // MVP has at most one table in the core spec, but this runtime storage can still count imported/local table
        // records.  Reference-types/table proposal work must revisit all_table_count consumers and call_indirect mem/table
        // immediates together.
        auto const all_table_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_table_count + local_table_count)};

        // memory
        auto const& memsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::memory_section_storage_t>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 2uz);
        auto const& imported_memories{importsec.importdesc.index_unchecked(2u)};
        auto const imported_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_memories.size())};
        auto const local_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(memsec.memories.size())};
        // WebAssembly 1.0/MVP memory load/store opcodes implicitly target memory 0.  Multi-memory support must add
        // explicit memory-index validation in the memory opcode cases and the LLVM emitter's memory access cache.
        auto const all_memory_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_memory_count + local_memory_count)};

        // Structured-control stack.  The initial frame is the implicit function block and is popped only after the final
        // `end` instruction validates the function result.
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

        // Polymorphic state is entered after unreachable/br/return-style instructions.  In that state, validation accepts
        // stack operations without concrete operands until the next structured boundary.
        bool is_polymorphic{};

        curr_runtime_virtual_register_id next_virtual_register_id{};

        auto const local_virtual_registers_push_back{
            [&](curr_operand_stack_value_type type) constexpr noexcept
            { local_virtual_registers.push_back(curr_local_virtual_register_t{.type = type, .virtual_register_id = next_virtual_register_id++}); }};

        // Assign stable virtual registers to parameters first, then declared locals, matching Wasm local-index order.
        for(::std::size_t i{}; i != func_parameter_count_uz; ++i) { local_virtual_registers_push_back(func_parameter_begin[i]); }
        for(auto const& local_part: curr_code_locals)
        {
            for(validation_module_traits_t::wasm_u32 i{}; i != local_part.count; ++i) { local_virtual_registers_push_back(local_part.type); }
        }

        if(local_virtual_registers.size() != static_cast<::std::size_t>(all_local_count)) [[unlikely]] { runtime_storage_bug(); }

        auto const local_virtual_register_from_index{
            [&](validation_module_traits_t::wasm_u32 local_index) constexpr noexcept -> curr_local_virtual_register_t const&
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
            [&](curr_operand_stack_value_type type) constexpr noexcept
            { operand_stack.push_back(runtime_operand_stack_storage_t{.type = type, .virtual_register_id = allocate_virtual_register()}); }};

        auto const operand_stack_pop_unchecked{[&]() constexpr noexcept -> runtime_operand_stack_storage_t
                                               {
                                                   if(operand_stack.empty()) [[unlikely]] { runtime_storage_bug(); }
                                                   auto const value{operand_stack.back()};
                                                   operand_stack.pop_back();
                                                   return value;
                                               }};

        auto const operand_stack_pop_n{[&](::std::size_t n) constexpr noexcept
                                       {
                                           while(n-- != 0uz && !operand_stack.empty()) { static_cast<void>(operand_stack_pop_unchecked()); }
                                       }};

        auto const operand_stack_truncate_to{[&](::std::size_t new_size) constexpr noexcept
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

        auto const try_pop_concrete_operand{[&]() constexpr noexcept -> concrete_operand_t
                                            {
                                                if(concrete_operand_count() == 0uz) { return {}; }
                                                auto const operand{operand_stack_pop_unchecked()};
                                                return {.from_stack = true, .type = operand.type};
                                            }};

        auto const try_peek_concrete_operand{[&]() constexpr noexcept -> concrete_operand_t
                                             {
                                                 if(concrete_operand_count() == 0uz) { return {}; }
                                                 return {.from_stack = true, .type = operand_stack.back().type};
                                             }};

        // block type
        using value_type_enum = curr_operand_stack_value_type;
        // WebAssembly 1.0/MVP blocktype encodes either empty or one value type directly.  Multi-value blocktypes use a
        // type index and must replace this fixed one-element-array fast path when that proposal is enabled.
        static constexpr value_type_enum i32_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32)};
        static constexpr value_type_enum i64_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64)};
        static constexpr value_type_enum f32_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32)};
        static constexpr value_type_enum f64_result_arr[1u]{static_cast<value_type_enum>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64)};

        // Function block (label/result type is the function result).  MVP functions have zero or one result; if
        // multi-value results are enabled, the validator can already carry a pointer range but the LLVM emitter's PHI and
        // return lowering must be extended in the same change.
        control_flow_stack.push_back({
            .result = {.begin = curr_func_type.result.begin, .end = curr_func_type.result.end},
            .operand_stack_base = 0uz,
            .type = block_type::function,
            .polymorphic_base = false,
            .then_polymorphic_end = false
        });

        // Start parsing the code body.
        auto code_curr{code_begin};

        runtime_local_func_llvm_jit_emit_state_t llvm_jit_emit_state{};
        // LLVM JIT emission is opportunistic.  If preparation fails, validation still runs and the caller can continue
        // with interpreter/runtime execution.
        bool emit_llvm_jit_active{emitted_llvm_jit_ir_storage != nullptr &&
                                  try_prepare_runtime_local_func_llvm_jit_emit_state(local_func_storage,
                                                                                     *emitted_llvm_jit_ir_storage,
                                                                                     llvm_jit_emit_state,
                                                                                     verify_llvm_jit_ir,
                                                                                     route_wasm_calls_through_runtime_bridge,
                                                                                     lazy_defined_raw_call_target_base_address,
                                                                                     lazy_defined_raw_call_target_count,
                                                                                     lazy_defined_typed_entry_target_base_address,
                                                                                     lazy_defined_typed_entry_target_count,
                                                                                     lazy_defined_targets_are_atomic,
                                                                                     emit_tiered_loop_reentry_entries,
                                                                                     emit_call_stack_frames,
                                                                                     emit_unwind_call_stack_frames)};

        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        using wasm1p1_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_basic;
        using wasm1p1_numeric_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_numeric;

        parser_feature_parameter_t const default_validator_feature_parameter{};
        auto const& effective_validator_feature_parameter{
            validator_feature_parameter == nullptr ? default_validator_feature_parameter : *validator_feature_parameter};
        [[maybe_unused]] auto const& wasm1p1_para{
            ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(effective_validator_feature_parameter)};

        auto const opcode_byte{[](wasm1p1_code opcode) constexpr noexcept -> validation_module_traits_t::wasm_u32
                               { return static_cast<validation_module_traits_t::wasm_u32>(static_cast<::std::uint_least8_t>(opcode)); }};

        auto const fail_wasm1p1_feature_required{
            [&](::std::byte const* op_begin,
                validation_module_traits_t::wasm_u32 value,
                ::uwvm2::parser::wasm::base::wasm1p1_feature_kind feature,
                ::uwvm2::parser::wasm::base::wasm1p1_error_subject subject) constexpr UWVM_THROWS
            {
                err.err_curr = op_begin;
                err.err_selectable.wasm1p1_feature_required.value = value;
                err.err_selectable.wasm1p1_feature_required.feature = feature;
                err.err_selectable.wasm1p1_feature_required.subject = subject;
                err.err_code = code_validation_error_code::wasm1p1_feature_required;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }};

        auto const fail_invalid_immediate{
            [&](::std::byte const* op_begin,
                ::uwvm2::utils::container::u8string_view op_name,
                ::fast_io::parse_code pc = ::fast_io::parse_code::invalid) constexpr UWVM_THROWS
            {
                err.err_curr = op_begin;
                err.err_selectable.invalid_const_immediate.op_code_name = op_name;
                err.err_code = code_validation_error_code::invalid_const_immediate;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(pc);
            }};

        auto const read_leb128{
            [&]<typename T>(::std::byte const*& curr,
                            ::std::byte const* end,
                            ::std::byte const* op_begin,
                            ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS -> T
            {
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                T value{};
                auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                 reinterpret_cast<char8_t_const_may_alias_ptr>(end),
                                                                 ::fast_io::mnp::leb128_get(value))};
                if(perr != ::fast_io::parse_code::ok) [[unlikely]] { fail_invalid_immediate(op_begin, op_name, perr); }

                curr = reinterpret_cast<::std::byte const*>(next);
                return value;
            }};

        auto const read_u8_immediate{
            [&](::std::byte const*& curr,
                ::std::byte const* end,
                ::std::byte const* op_begin,
                ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS -> ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
            {
                if(curr == end) [[unlikely]] { fail_invalid_immediate(op_begin, op_name, ::fast_io::parse_code::end_of_file); }

                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte value{};
                ::std::memcpy(::std::addressof(value), curr, sizeof(value));
#if CHAR_BIT > 8
                value = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(static_cast<::std::uint_least8_t>(value) & 0xFFu);
#endif
                ++curr;
                return value;
            }};

        auto const ensure_wasm1p1_value_type_enabled{
            [&](::std::byte const* op_begin,
                curr_operand_stack_value_type type,
                ::uwvm2::parser::wasm::base::wasm1p1_error_subject subject) constexpr UWVM_THROWS
            {
                auto const vt{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>(type)};
                if(!::uwvm2::parser::wasm::standard::wasm1p1::type::is_valid_value_type(vt)) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.wasm1p1_invalid_reference_type.value =
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(type);
                    err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                if(!::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(vt, effective_validator_feature_parameter)) [[unlikely]]
                {
                    auto const feature{vt == ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128
                                           ? ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd
                                           : ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types};
                    fail_wasm1p1_feature_required(
                        op_begin, static_cast<validation_module_traits_t::wasm_u32>(static_cast<::std::uint_least8_t>(type)), feature, subject);
                }
            }};

        // Shared validator for unary numeric opcodes.  The opcode case file supplies the expected operand/result MVP
        // scalar type, while this helper handles stack-polymorphic behavior and diagnostics.
        auto const validate_numeric_unary_stack_effect{
            [&](::std::byte const* op_begin,
                ::uwvm2::utils::container::u8string_view op_name,
                curr_operand_stack_value_type expected_operand_type,
                curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
            {
                if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]]
                {
                    report_operand_stack_underflow(op_begin, op_name, 1uz);
                }

                // In polymorphic code the operand may be absent.  Only concrete operands are type-checked; the result is
                // still pushed to keep later validation stack shapes consistent.
                auto const operand{try_pop_concrete_operand()};

                if(operand.from_stack && operand.type != expected_operand_type) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = static_cast<wasm_value_type>(expected_operand_type);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = static_cast<wasm_value_type>(operand.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                operand_stack_push(result_type);
            }};

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

                                              validate_numeric_unary_stack_effect(op_begin, op_name, expected_operand_type, result_type);
                                          }};

        auto const validate_numeric_binary{
            [&](::uwvm2::utils::container::u8string_view op_name,
                curr_operand_stack_value_type expected_operand_type,
                curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
            {
                // Binary numeric opcodes pop RHS first, then LHS, matching WebAssembly stack-machine order.
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

                // Check each concrete operand separately so diagnostics can report the actual mismatched type even when
                // only one side is wrong.
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
                                         // WebAssembly 1.0/MVP memory loads have no memory-index immediate and always
                                         // target memory 0.  Multi-memory support must add memidx decoding/validation
                                         // here and pass that index to the LLVM memory emitter.
                                         auto const op_begin{code_curr};
                                         ++code_curr;

                                         ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                                         ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                                         using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                         // Decode the MVP memarg pair: alignment exponent followed by static offset.
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

                                         // MVP requires at least a default memory for every memory load/store.  With
                                         // multi-memory this check must become "selected memory index exists".
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

                                         if(auto const addr{try_pop_concrete_operand()}; addr.from_stack && addr.type != curr_operand_stack_value_type::i32)
                                             [[unlikely]]
                                         {
                                             // MVP linear memory addresses are i32.  Memory64 must relax this to the
                                             // selected memory's address type and update LLVM effective-address lowering.
                                             err.err_curr = op_begin;
                                             err.err_selectable.memarg_address_type_not_i32.op_code_name = op_name;
                                             err.err_selectable.memarg_address_type_not_i32.addr_type = to_wasm1_diagnostic_value_type(addr.type);
                                             err.err_code = code_validation_error_code::memarg_address_type_not_i32;
                                             ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                         }

                                         operand_stack_push(result_type);
                                     }};

        auto const validate_mem_store{[&](::uwvm2::utils::container::u8string_view op_name,
                                          ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const max_align,
                                          curr_operand_stack_value_type const expected_value_type) constexpr UWVM_THROWS
                                      {
                                          // WebAssembly 1.0/MVP memory stores also target implicit memory 0.  Future
                                          // multi-memory support must mirror the load-side memidx work here.
                                          auto const op_begin{code_curr};
                                          ++code_curr;

                                          ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 align;   // No initialization necessary
                                          ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 offset;  // No initialization necessary

                                          using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                          // Decode the MVP memarg pair before touching the operand stack so parse errors
                                          // point at the memory instruction rather than later stack diagnostics.
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

                                          auto const [offset_next,
                                                      offset_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                           reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                           ::fast_io::mnp::leb128_get(offset))};
                                          if(offset_err != ::fast_io::parse_code::ok) [[unlikely]]
                                          {
                                              err.err_curr = op_begin;
                                              err.err_code = code_validation_error_code::invalid_memarg_offset;
                                              ::uwvm2::parser::wasm::base::throw_wasm_parse_code(offset_err);
                                          }

                                          code_curr = reinterpret_cast<::std::byte const*>(offset_next);

                                          // MVP default-memory existence check.  Multi-memory must validate the decoded
                                          // memory index instead of only checking that some memory exists.
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
                                              // MVP and current LLVM lowering require i32 addresses.  Memory64 support
                                              // must use the selected memory address type.
                                              err.err_curr = op_begin;
                                              err.err_selectable.memarg_address_type_not_i32.op_code_name = op_name;
                                              err.err_selectable.memarg_address_type_not_i32.addr_type = to_wasm1_diagnostic_value_type(addr.type);
                                              err.err_code = code_validation_error_code::memarg_address_type_not_i32;
                                              ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                          }

                                          if(value.from_stack && value.type != expected_value_type) [[unlikely]]
                                          {
                                              // Store value type is checked after address type so diagnostics follow the
                                              // Wasm operand order: address below value on the stack.
                                              err.err_curr = op_begin;
                                              err.err_selectable.store_value_type_mismatch.op_code_name = op_name;
                                              err.err_selectable.store_value_type_mismatch.expected_type = static_cast<wasm_value_type>(expected_value_type);
                                              err.err_selectable.store_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(value.type);
                                              err.err_code = code_validation_error_code::store_value_type_mismatch;
                                              ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                          }
                                      }};

#include "single_func_validation_dispatch.h"
    }

    // Build the borrowed storage descriptor for a local defined function by local-function index.
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
        // Function type/code pointers are finalized runtime invariants.  A null pointer here is host/runtime corruption,
        // not a Wasm validation error in the guest module.
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

    inline constexpr void validate_runtime_local_func_with_standard_wasm1p1_validator(
        validation_module_storage_t const& validation_module,
        local_func_storage_t const& local_func_storage,
        ::uwvm2::validation::error::code_validation_error_impl& err,
        parser_feature_parameter_t const* validator_feature_parameter) UWVM_THROWS
    {
        parser_feature_parameter_t const default_validator_feature_parameter{};
        auto const& effective_validator_feature_parameter{
            validator_feature_parameter == nullptr ? default_validator_feature_parameter : *validator_feature_parameter};
        ::uwvm2::validation::standard::wasm1p1::validate_code(::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
                                                              validation_module,
                                                              local_func_storage.function_index,
                                                              local_func_storage.code_begin,
                                                              local_func_storage.code_end,
                                                              err,
                                                              effective_validator_feature_parameter);
    }

    [[nodiscard]] inline constexpr bool runtime_local_func_requires_interpreter_fallback(
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
        local_func_storage_t const& local_func_storage) noexcept
    {
        auto const function_type_ptr{local_func_storage.function_type_ptr};
        auto const wasm_code_ptr{local_func_storage.wasm_code_ptr};
        if(function_type_ptr == nullptr || wasm_code_ptr == nullptr || local_func_storage.code_begin == nullptr ||
           local_func_storage.code_end == nullptr) [[unlikely]]
        {
            return true;
        }
        if(!is_runtime_wasm_function_type_inline_llvm_jit_supported(*function_type_ptr)) { return true; }

        for(auto const& local_part: wasm_code_ptr->locals)
        {
            if(!is_runtime_wasm_value_type_inline_llvm_jit_scalar(static_cast<runtime_operand_stack_value_type>(local_part.type))) { return true; }
        }

        auto const is_inline_scalar_blocktype_byte{[](::std::uint_least8_t blocktype_byte) constexpr noexcept
                                                   {
                                                       if(blocktype_byte == 0x40u) { return true; }
                                                       auto const vt{static_cast<runtime_operand_stack_value_type>(blocktype_byte)};
                                                       return is_runtime_wasm_value_type_inline_llvm_jit_scalar(vt);
                                                   }};

        auto code_curr{local_func_storage.code_begin};
        auto const code_end{local_func_storage.code_end};
        while(code_curr < code_end)
        {
            ::std::uint_least8_t curr_opcode_byte{};
            ::std::memcpy(::std::addressof(curr_opcode_byte), code_curr, sizeof(curr_opcode_byte));

            switch(curr_opcode_byte)
            {
                case static_cast<::std::uint_least8_t>(wasm1_code::block):
                case static_cast<::std::uint_least8_t>(wasm1_code::loop):
                case static_cast<::std::uint_least8_t>(wasm1_code::if_):
                {
                    ++code_curr;
                    if(code_curr == code_end) [[unlikely]] { return true; }
                    ::std::uint_least8_t blocktype_byte{};
                    ::std::memcpy(::std::addressof(blocktype_byte), code_curr, sizeof(blocktype_byte));
                    ++code_curr;
                    if(!is_inline_scalar_blocktype_byte(blocktype_byte)) { return true; }
                    break;
                }
                case static_cast<::std::uint_least8_t>(wasm1_code::call):
                {
                    ++code_curr;
                    validation_module_traits_t::wasm_u32 func_index{};
                    if(!parse_wasm_leb128_immediate(code_curr, code_end, func_index)) [[unlikely]] { return true; }
                    auto const callee_type_ptr{resolve_runtime_callee_function_type(curr_module, func_index)};
                    if(callee_type_ptr == nullptr || !is_runtime_wasm_function_type_inline_llvm_jit_supported(*callee_type_ptr)) { return true; }
                    break;
                }
                case static_cast<::std::uint_least8_t>(wasm1_code::call_indirect):
                {
                    ++code_curr;
                    validation_module_traits_t::wasm_u32 type_index{};
                    validation_module_traits_t::wasm_u32 table_index{};
                    if(!parse_wasm_leb128_immediate(code_curr, code_end, type_index) ||
                       !parse_wasm_leb128_immediate(code_curr, code_end, table_index)) [[unlikely]]
                    {
                        return true;
                    }
                    static_cast<void>(table_index);
                    auto const callee_type_ptr{resolve_runtime_type_section_function_type(curr_module, type_index)};
                    if(callee_type_ptr == nullptr || !is_runtime_wasm_function_type_inline_llvm_jit_supported(*callee_type_ptr)) { return true; }
                    break;
                }
                case static_cast<::std::uint_least8_t>(wasm1p1_code::select_t):
                {
                    ++code_curr;
                    validation_module_traits_t::wasm_u32 result_type_count{};
                    if(!parse_wasm_leb128_immediate(code_curr, code_end, result_type_count)) [[unlikely]] { return true; }
                    if(result_type_count == 0u) { break; }
                    if(result_type_count != 1u || code_curr == code_end) [[unlikely]] { return true; }
                    ::std::uint_least8_t result_type_byte{};
                    ::std::memcpy(::std::addressof(result_type_byte), code_curr, sizeof(result_type_byte));
                    ++code_curr;
                    if(!is_runtime_wasm_value_type_inline_llvm_jit_scalar(static_cast<runtime_operand_stack_value_type>(result_type_byte))) { return true; }
                    break;
                }
                case static_cast<::std::uint_least8_t>(wasm1p1_code::i32_extend8_s):
                case static_cast<::std::uint_least8_t>(wasm1p1_code::i32_extend16_s):
                case static_cast<::std::uint_least8_t>(wasm1p1_code::i64_extend8_s):
                case static_cast<::std::uint_least8_t>(wasm1p1_code::i64_extend16_s):
                case static_cast<::std::uint_least8_t>(wasm1p1_code::i64_extend32_s):
                {
                    ++code_curr;
                    break;
                }
                case static_cast<::std::uint_least8_t>(wasm1p1_code::numeric_prefix):
                {
                    ++code_curr;
                    validation_module_traits_t::wasm_u32 subopcode{};
                    if(!parse_wasm_leb128_immediate(code_curr, code_end, subopcode)) [[unlikely]] { return true; }
                    switch(static_cast<wasm1p1_numeric_code>(subopcode))
                    {
                        case wasm1p1_numeric_code::i32_trunc_sat_f32_s:
                        case wasm1p1_numeric_code::i32_trunc_sat_f32_u:
                        case wasm1p1_numeric_code::i32_trunc_sat_f64_s:
                        case wasm1p1_numeric_code::i32_trunc_sat_f64_u:
                        case wasm1p1_numeric_code::i64_trunc_sat_f32_s:
                        case wasm1p1_numeric_code::i64_trunc_sat_f32_u:
                        case wasm1p1_numeric_code::i64_trunc_sat_f64_s:
                        case wasm1p1_numeric_code::i64_trunc_sat_f64_u:
                        {
                            break;
                        }
                        default:
                        {
                            return true;
                        }
                    }
                    break;
                }
                case static_cast<::std::uint_least8_t>(wasm1p1_code::table_get):
                case static_cast<::std::uint_least8_t>(wasm1p1_code::table_set):
                case static_cast<::std::uint_least8_t>(wasm1p1_code::ref_null):
                case static_cast<::std::uint_least8_t>(wasm1p1_code::ref_is_null):
                case static_cast<::std::uint_least8_t>(wasm1p1_code::ref_func):
                case static_cast<::std::uint_least8_t>(wasm1p1_code::simd_prefix):
                {
                    return true;
                }
                default:
                {
                    if(!skip_wasm_unreachable_noncontrol_instruction(code_curr, code_end)) [[unlikely]] { return true; }
                    break;
                }
            }
        }

        return code_curr != code_end;
    }

    // Validate one local function and optionally append its LLVM IR into the supplied module storage.
    inline constexpr local_func_storage_t compile_all_from_uwvm_local_func(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                           validation_module_storage_t const& validation_module,
                                                                           [[maybe_unused]] compile_option const& options,
                                                                           ::std::size_t local_function_idx,
                                                                           ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                           llvm_jit_module_storage_t* emitted_llvm_jit_ir_storage = nullptr) UWVM_THROWS
    {
        local_func_storage_t local_func_storage{get_runtime_local_func_storage(curr_module, local_function_idx, err)};
        local_func_storage.module_id = options.curr_wasm_id;
        validate_runtime_local_func_with_standard_wasm1p1_validator(validation_module, local_func_storage, err, options.validator_feature_parameter);

        auto const requires_interpreter_fallback{runtime_local_func_requires_interpreter_fallback(curr_module, local_func_storage)};
        if(emitted_llvm_jit_ir_storage == nullptr && requires_interpreter_fallback) { return local_func_storage; }

        auto const use_interpreter_fallback{emitted_llvm_jit_ir_storage != nullptr && requires_interpreter_fallback};
        if(use_interpreter_fallback)
        {
            if(!emit_runtime_local_func_llvm_jit_interpreter_fallback_entries(local_func_storage,
                                                                              *emitted_llvm_jit_ir_storage,
                                                                              options.verify_llvm_jit_ir,
                                                                              options.emit_unwind_call_stack_frames)) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }
            return local_func_storage;
        }

        // Validation writes tiered loop reentry metadata back into the returned local_func_storage, so callers can publish
        // OSR entry information together with the compiled function metadata.
        validate_runtime_local_func(validation_module,
                                    local_func_storage,
                                    err,
                                    emitted_llvm_jit_ir_storage,
                                    options.verify_llvm_jit_ir,
                                    options.route_wasm_calls_through_runtime_bridge,
                                    options.lazy_defined_raw_call_target_base_address,
                                    options.lazy_defined_raw_call_target_count,
                                    options.lazy_defined_typed_entry_target_base_address,
                                    options.lazy_defined_typed_entry_target_count,
                                    options.lazy_defined_targets_are_atomic,
                                    options.emit_tiered_loop_reentry_entries,
                                    options.emit_call_stack_frames,
                                    options.emit_unwind_call_stack_frames,
                                    options.validator_feature_parameter,
                                    ::std::addressof(local_func_storage.tiered_loop_reentries));
        return local_func_storage;
    }

    // Compile/validate every local function in one half-open task group.
    inline constexpr void compile_all_from_uwvm_local_func_group(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                 validation_module_storage_t const& validation_module,
                                                                 [[maybe_unused]] compile_option const& options,
                                                                 full_function_symbol_t& storage,
                                                                 local_function_task_group task_group,
                                                                 ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                 llvm_jit_module_storage_t* emitted_llvm_jit_ir_storage = nullptr) UWVM_THROWS
    {
        for(::std::size_t local_function_idx{task_group.begin_index}; local_function_idx != task_group.end_index; ++local_function_idx)
        {
            storage.local_funcs.index_unchecked(local_function_idx) =
                compile_all_from_uwvm_local_func(curr_module, validation_module, options, local_function_idx, err, emitted_llvm_jit_ir_storage);
        }
    }

    // Shared failure state for parallel compilation.  Only the first failing worker publishes the diagnostic/exception.
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
    // Publish the first parallel compilation failure using acquire/release synchronization so the scheduler thread sees a
    // fully written diagnostic or exception pointer.
    inline constexpr void publish_parallel_compile_failure(parallel_compile_failure_state& failure_state,
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

    // Coroutine task wrapper used by the uwvm thread pool for one function group.
    inline ::uwvm2::utils::thread::scheduled_task
        make_compile_all_from_uwvm_local_func_group_task(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                         validation_module_storage_t const& validation_module,
                                                         [[maybe_unused]] compile_option const& options,
                                                         full_function_symbol_t& storage,
                                                         parallel_compile_failure_state& failure_state,
                                                         local_function_task_group task_group,
                                                         llvm_jit_module_storage_t* emitted_llvm_jit_ir_storage = nullptr) noexcept
    {
#ifdef UWVM_CPP_EXCEPTIONS
        if(failure_state.failed.load(::std::memory_order_acquire)) { co_return; }
#endif

        ::uwvm2::validation::error::code_validation_error_impl local_err{};

#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            // Each worker gets a task-local LLVM module fragment.  Fragments are linked after all workers finish so LLVM
            // objects do not cross contexts during concurrent emission.
            compile_all_from_uwvm_local_func_group(curr_module, validation_module, options, storage, task_group, local_err, emitted_llvm_jit_ir_storage);
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

    inline ::uwvm2::utils::thread::scheduled_task make_llvm_jit_task_module_pre_link_callback_task(llvm_jit_module_storage_t& task_module_storage,
                                                                                                   compile_option const& options,
                                                                                                   ::std::atomic_bool& failed) noexcept
    {
        if(failed.load(::std::memory_order_acquire)) { co_return; }

        // The callback can run optimizer/pre-link customization on a fragment.  A false return disables the parallel
        // fragment path and makes the caller fall back to serial compilation.
        if(options.llvm_jit_task_module_pre_link_callback != nullptr &&
           !options.llvm_jit_task_module_pre_link_callback(task_module_storage, options.llvm_jit_task_module_pre_link_callback_context))
        {
            failed.store(true, ::std::memory_order_release);
        }

        co_return;
    }

    [[nodiscard]] inline constexpr bool
        run_llvm_jit_task_module_pre_link_callback(::uwvm2::utils::container::vector<llvm_jit_module_storage_t>& task_module_storages,
                                                   compile_option const& options,
                                                   ::std::size_t effective_extra_compile_threads) noexcept
    {
        // No callback or no fragments means there is nothing to optimize before linking.
        if(options.llvm_jit_task_module_pre_link_callback == nullptr) { return true; }
        if(task_module_storages.empty()) { return true; }

        ::std::atomic_bool failed{};
        ::uwvm2::utils::thread::scheduled_task_batch task_batch{task_module_storages.size()};
        for(auto& task_module_storage: task_module_storages)
        {
            auto task{make_llvm_jit_task_module_pre_link_callback_task(task_module_storage, options, failed)};
            ::std::construct_at(task_batch.handles.buffer + task_batch.handle_count, task.release());
            ++task_batch.handle_count;
        }

        ::uwvm2::utils::thread::native_thread_pool thread_pool{};
        thread_pool.run(task_batch, effective_extra_compile_threads);
        return !failed.load(::std::memory_order_acquire);
    }

    [[nodiscard]] inline constexpr bool
        link_llvm_jit_module_fragments(llvm_jit_module_storage_t& merged_module_storage,
                                       ::uwvm2::utils::container::vector<llvm_jit_module_storage_t>& task_module_storages) noexcept
    {
        if(merged_module_storage.llvm_context_holder == nullptr || merged_module_storage.llvm_module == nullptr) [[unlikely]] { return false; }

        // LLVM modules produced in different contexts cannot be linked directly with all toolchains.  Serialize each
        // fragment to bitcode and parse it into the merged context first.
        auto& merged_llvm_context{*merged_module_storage.llvm_context_holder};
        ::llvm::Linker linker(*merged_module_storage.llvm_module);
        for(auto& task_module_storage: task_module_storages)
        {
            if(task_module_storage.llvm_context_holder == nullptr || task_module_storage.llvm_module == nullptr) [[unlikely]] { return false; }

            ::uwvm2::utils::container::u8string serialized_bitcode{};
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
            // Drop the fragment after successful linking so later finalization only sees the merged module.
            task_module_storage.llvm_module.reset();
            task_module_storage.llvm_context_holder.reset();
            task_module_storage.emitted = false;
        }

        return true;
    }

    inline constexpr bool finalize_runtime_llvm_jit_module_storage(llvm_jit_module_storage_t& module_storage, bool verify_llvm_jit_ir) noexcept
    {
        module_storage.emitted = module_storage.llvm_context_holder != nullptr && module_storage.llvm_module != nullptr;
        if(!module_storage.emitted) { return true; }

        // DIBuilder must be finalized before module verification so debug metadata is complete.
        if(module_storage.llvm_di_builder != nullptr) { module_storage.llvm_di_builder->finalize(); }

        if(!verify_llvm_jit_module(*module_storage.llvm_module, verify_llvm_jit_ir)) [[unlikely]]
        {
            module_storage = {};
            return false;
        }

        return true;
    }

    inline constexpr void validate_runtime_module_all_local_funcs(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                  ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        // Public validation-only entry used when the runtime already has finalized module storage and no LLVM output is
        // requested.
        auto const validation_module{build_runtime_validation_module(curr_module)};
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};

        for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
        {
            auto const local_func_storage{get_runtime_local_func_storage(curr_module, local_function_idx, err)};
            validate_runtime_local_func_with_standard_wasm1p1_validator(validation_module, local_func_storage, err, nullptr);
        }
    }
}  // namespace details

inline constexpr ::std::size_t default_small_module_code_size_threshold{512uz * 1024uz};
inline constexpr ::std::size_t default_target_task_groups_per_compile_thread{4uz};
inline constexpr ::std::size_t default_target_task_groups_per_adjusted_compile_thread{4uz};
inline constexpr ::std::size_t aggressive_target_task_groups_per_adjusted_compile_thread{5uz};

    // Validate all code bodies in a parser-owned module and print a formatted diagnostic on the first failure.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr bool
        validate_all_wasm_code_for_module(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
                                          ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& feature_parameter,
                                          ::uwvm2::utils::container::u8cstring_view file_name,
                                          ::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        details::validation_module_storage_t const& validation_module{module_storage};

    auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<details::validation_module_traits_t::import_section_storage_t>(
        validation_module.sections)};
    // Function indices in Wasm validation include imported functions before local code-section bodies.
    auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};

    auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<details::validation_module_traits_t::code_section_storage_t>(
        validation_module.sections)};

    for(::std::size_t local_idx{}; local_idx < codesec.codes.size(); ++local_idx)
    {
        // Parser-owned code bodies already have section-local indices; convert to module function index by adding imports.
        auto const& code{codesec.codes.index_unchecked(local_idx)};
        auto const code_begin_ptr{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
        auto const code_end_ptr{reinterpret_cast<::std::byte const*>(code.body.code_end)};

        ::uwvm2::validation::error::code_validation_error_impl v_err{};
#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            ::uwvm2::validation::standard::wasm1p1::validate_code(::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
                                                                  validation_module,
                                                                          import_func_count + local_idx,
                                                                          code_begin_ptr,
                                                                          code_end_ptr,
                                                                          v_err,
                                                                          feature_parameter);
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

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr bool
        validate_all_wasm_code_for_module(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
                                          ::uwvm2::utils::container::u8cstring_view file_name,
                                          ::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> default_feature_parameter{};
        return validate_all_wasm_code_for_module(module_storage, default_feature_parameter, file_name, module_name);
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
        // Only Wasm modules contain code bodies for this validator.  Local imports, dynamic libraries, and weak symbols
        // provide host/runtime functions instead.
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
                        // Binary format version 1 is the WebAssembly 1.0/MVP-compatible format handled by this path.
                        // A future binfmt version must add a separate validation adapter instead of falling through here.
                        if(!validate_all_wasm_code_for_module(wf->wasm_module_storage.wasm_binfmt_ver1_storage,
                                                              wf->wasm_parameter.binfmt1_para,
                                                              wf->file_name,
                                                              module_name))
                        {
                            return false;
                        }
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

// Adjust the default code-size split for small modules so parallel compilation does not create too many tiny tasks.
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

    // Round up so every byte is assigned and the final group is not systematically oversized.
    auto const adaptive_split_size{total_code_size / target_task_group_count + static_cast<::std::size_t>(total_code_size % target_task_group_count != 0uz)};

    if(adaptive_split_size > split_config.split_size) { split_config.split_size = adaptive_split_size; }
    return split_config;
}

// Optionally reduce extra worker count after task splitting so the pool is not larger than the useful task count.
[[nodiscard]] inline constexpr ::std::size_t
    resolve_effective_adaptive_extra_compile_threads(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
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

// Validate and compile every local function in a runtime module, using serial or parallel LLVM emission depending on the
// task split configuration and available worker count.
inline constexpr full_function_symbol_t compile_all_from_uwvm(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
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
        [&]() constexpr UWVM_THROWS
        {
            // Serial mode emits all functions into one module and is also the fallback path when parallel preparation,
            // pre-link optimization, linking, or verification fails.
            auto const emit_llvm_jit_active{
                details::try_prepare_runtime_llvm_jit_module_storage(curr_module, storage.llvm_jit_module, options.emit_unwind_call_stack_frames)};
            for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
            {
                storage.local_funcs.index_unchecked(local_function_idx) =
                    details::compile_all_from_uwvm_local_func(curr_module,
                                                              validation_module,
                                                              options,
                                                              local_function_idx,
                                                              err,
                                                              emit_llvm_jit_active ? ::std::addressof(storage.llvm_jit_module) : nullptr);
            }
            static_cast<void>(details::finalize_runtime_llvm_jit_module_storage(storage.llvm_jit_module, options.verify_llvm_jit_ir));
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

    if(!details::try_prepare_runtime_llvm_jit_module_storage(curr_module, storage.llvm_jit_module, options.emit_unwind_call_stack_frames))
    {
        compile_local_functions_serially();
        return storage;
    }

    ::uwvm2::utils::container::vector<llvm_jit_module_storage_t> task_llvm_jit_modules{};
    task_llvm_jit_modules.resize(task_groups.size());

    bool prepared_task_llvm_jit_modules{true};
    for(auto& task_llvm_jit_module: task_llvm_jit_modules)
    {
        if(!details::try_prepare_runtime_llvm_jit_module_storage(curr_module, task_llvm_jit_module, options.emit_unwind_call_stack_frames))
        {
            prepared_task_llvm_jit_modules = false;
            break;
        }
    }

    if(!prepared_task_llvm_jit_modules)
    {
        // If any fragment module cannot be prepared, restart serially so validation/compilation still produces a
        // coherent result instead of a partially parallel output.
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

    bool llvm_jit_task_modules_pre_link_optimized{};
    if(options.llvm_jit_task_module_pre_link_callback != nullptr)
    {
        if(!details::run_llvm_jit_task_module_pre_link_callback(task_llvm_jit_modules, options, effective_extra_compile_threads))
        {
            compile_local_functions_serially();
            return storage;
        }
        llvm_jit_task_modules_pre_link_optimized = true;
    }

    if(!details::link_llvm_jit_module_fragments(storage.llvm_jit_module, task_llvm_jit_modules))
    {
        // Linking failure is treated as an implementation/runtime issue in the parallel path; fall back to serial emission
        // instead of returning a half-linked module.
        storage.llvm_jit_task_modules_pre_link_optimized = false;
        compile_local_functions_serially();
        return storage;
    }

    if(!details::finalize_runtime_llvm_jit_module_storage(storage.llvm_jit_module, options.verify_llvm_jit_ir))
    {
        storage.llvm_jit_task_modules_pre_link_optimized = false;
        compile_local_functions_serially();
        return storage;
    }

    storage.llvm_jit_task_modules_pre_link_optimized = llvm_jit_task_modules_pre_link_optimized;
    return storage;
}

// Convenience path used when only the first local function should be compiled.
inline constexpr full_function_symbol_t compile_all_from_uwvm_single_func(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                          [[maybe_unused]] compile_option& options,
                                                                          ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
{
    full_function_symbol_t storage{};
    if(curr_module.local_defined_function_vec_storage.empty()) { return storage; }
    auto const validation_module{details::build_runtime_validation_module(curr_module)};

    storage.local_funcs.reserve(1uz);
    auto const emit_llvm_jit_active{
        details::try_prepare_runtime_llvm_jit_module_storage(curr_module, storage.llvm_jit_module, options.emit_unwind_call_stack_frames)};
    storage.local_funcs.push_back(details::compile_all_from_uwvm_local_func(curr_module,
                                                                            validation_module,
                                                                            options,
                                                                            0uz,
                                                                            err,
                                                                            emit_llvm_jit_active ? ::std::addressof(storage.llvm_jit_module) : nullptr));
    static_cast<void>(details::finalize_runtime_llvm_jit_module_storage(storage.llvm_jit_module, options.verify_llvm_jit_ir));
    return storage;
}
