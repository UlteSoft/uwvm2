// This header coordinates validation and optional LLVM JIT emission for local functions stored in the uwvm runtime module
// representation.  The code is header-only because the validator includes opcode-family case files and the LLVM emitter
// uses many local helper lambdas; keeping them in one translation context avoids a wide state-passing interface.
//
// Guest validity and LLVM lowering share one traversal of the function body.
// Core 3 type, feature, and local initialization rules use the same helpers as
// validation/standard/wasm3; the typed stack also supplies code-generation state.

// Metadata for one tiered/OSR loop entry that can re-enter a compiled function at a validated loop boundary.
struct tiered_loop_reentry_storage_t
{
    // Function-relative byte offset of the Wasm instruction that starts the reentry target.
    ::std::size_t wasm_code_offset{};

    // Non-zero id passed to the tiered core dispatcher.  Entry id zero is reserved for normal function entry.
    ::std::uint_least32_t entry_id{};
};

// Borrowed runtime storage needed to validate and optionally emit one local defined function.
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
#include "single_func_native_eh_private_leaf_metadata.h"
#endif
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
#include "single_func_native_eh_leaf_observer.h"
#endif
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

    // Cold compiler-only borrow. The native caller retains the exact nonmoving
    // registry through every worker/join. No generated instruction contains
    // this pointer, and it grants no source seal, epoch or execution permission.
    ::uwvm2::uwvm::runtime::storage::runtime_registry_type const* compiler_registry{};

#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    // Cold per-function work survives this actual fused call only.
    ::std::shared_ptr<native_eh_leaf_observer::function_state> native_eh_leaf_work{};
    ::std::shared_ptr<native_eh_leaf_observer::function_result const> native_eh_leaf_observation{};

#endif
    // OSR entries discovered while validating/emitting this function.
    ::uwvm2::utils::container::vector<tiered_loop_reentry_storage_t> tiered_loop_reentries{};

    // Debug-full only: one bit per expression byte, set only after the LLVM
    // safe-point bridge call for that exact Wasm opcode has been emitted.
    // Ordinary full/lazy/tiered compilation leaves this vector empty.
    ::uwvm2::utils::container::vector<::std::uint_least8_t> debug_safe_point_bits{};

    // Canonical runtime publication owns this immutable DATA after the same
    // fused validation/emission succeeds. Neither it nor its ordinals grant
    // pause, native-root, host-effect or executable resume authority.
    ::uwvm2::runtime::checkpoint::sealed_function_plan::owner checkpoint_plan{};
};

// Cold compiler DATA for the first decline in this module/fragment attempt.
// This record is not guest validity, executable publication, or stop authority.
// Serial fallback starts a fresh attempt; workers own separate records.
enum class llvm_jit_compiler_decline_stage : unsigned
{ none, module_prepare, function_prepare, instruction_or_function_finish, module_finalize, checkpoint_seal };

struct llvm_jit_compiler_first_decline
{
    llvm_jit_compiler_decline_stage stage{};
    ::std::size_t function_index{SIZE_MAX};
    ::std::size_t wasm_offset{SIZE_MAX};
    unsigned primary_opcode{static_cast<unsigned>(-1)};
    [[nodiscard]] inline constexpr bool available() const noexcept
    { return stage != llvm_jit_compiler_decline_stage::none; }
};

// LLVM objects owned by one emitted module fragment or by the final merged module.
struct llvm_jit_module_storage_t
{
    // True after finalization succeeds and the module/context pair is ready for optimization or execution.
    bool emitted{};

    // LLVM context must outlive the module, functions, and all LLVM types/values owned by the module.
    ::uwvm2::utils::container::delete_owned_ptr<::llvm::LLVMContext> llvm_context_holder{};

    // LLVM IR module for a full compile or a per-task fragment.
    ::uwvm2::utils::container::delete_owned_ptr<::llvm::Module> llvm_module{};

    // Declarations borrow this module, never host pointer slots. The qualified
    // TargetMachine is supplied before emission; runtime binds actual ABI symbols
    // in each ExecutionEngine before relocating or publishing native entries.
    ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::declarations native_exception_imports{};

    // Compilation-only scalars: never emitted as guest code or read on a guest access.
    llvm_jit_compiler_first_decline first_decline{};

    inline constexpr void note_first_decline(llvm_jit_compiler_decline_stage stage,
        ::std::size_t function_index = SIZE_MAX, ::std::size_t wasm_offset = SIZE_MAX,
        unsigned primary_opcode = static_cast<unsigned>(-1)) noexcept
    {
        if(!first_decline.available() && stage != llvm_jit_compiler_decline_stage::none)
        { first_decline = {stage, function_index, wasm_offset, primary_opcode}; }
    }

    inline constexpr void discard_emission_preserving_first_decline() noexcept
    {
        auto const owned{first_decline};
        *this = {};
        first_decline = owned;
    }

    inline constexpr llvm_jit_module_storage_t() noexcept = default;
    inline constexpr llvm_jit_module_storage_t(llvm_jit_module_storage_t const&) noexcept = delete;
    inline constexpr llvm_jit_module_storage_t& operator= (llvm_jit_module_storage_t const&) noexcept = delete;
    inline constexpr llvm_jit_module_storage_t(llvm_jit_module_storage_t&&) noexcept = default;

    inline constexpr llvm_jit_module_storage_t& operator= (llvm_jit_module_storage_t&& other) noexcept
    {
        if(this == ::std::addressof(other)) [[unlikely]] { return *this; }

        // Destroy dependent LLVM objects before replacing the context. LLVM IR objects keep context-owned uniqued types,
        // so resetting in dependency order avoids dangling ownership during move assignment.
        llvm_module.reset();
        llvm_context_holder.reset();

        first_decline = other.first_decline;
        other.first_decline = {};
        emitted = other.emitted;
        llvm_context_holder = ::std::move(other.llvm_context_holder);
        llvm_module = ::std::move(other.llvm_module);
        native_exception_imports = other.native_exception_imports;
        other.native_exception_imports = {};
        other.emitted = false;
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

#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    native_eh_leaf_observer::module_result native_eh_leaf_observations{};

#endif
    // Final merged LLVM module, or the serially emitted module when no parallel split is used.
    llvm_jit_module_storage_t llvm_jit_module{};
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
    // Independent prepublication IR/context. Only the separate actual full
    // publisher may bind a private engine after native extent/CFI validation.
    ::std::shared_ptr<native_eh_private_leaf::staged_module const> staged_native_eh_private_leaf{};
#endif

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

// Explicit provenance is required only by optional full-only instrumentation.
// T2 also uses this emitter, so its ordinary full-module layout is not evidence
// that the caller selected the standalone full execution mode.
enum class llvm_jit_compilation_mode : unsigned { unspecified, full, lazy, tiered };

// Entry/loop points support cooperative pause. Per-instruction observation is
// explicit debugging instrumentation and may inhibit ordinary IR optimizations.
enum class llvm_jit_debug_safe_point_granularity : unsigned { entry_loop, instruction };

// Compile-time-only borrow of the same fused validator's local initialization
// state. It cannot outlive this validate/emit call or enter generated Wasm.
struct llvm_jit_debug_local_initialization_query
{
    void const* context{};
    bool (*initialized)(void const*, ::std::size_t, bool&) noexcept{};
};

// User/runtime options controlling validation-time LLVM JIT emission.
struct compile_option
{
    // Runtime module id used by generated diagnostics.
    ::std::size_t curr_wasm_id{};

    // Explicit metadata world for private off-world compilation. Null retains
    // ordinary initialization's actual active registry. The owner must retain
    // the registry and resolved aliases unchanged until all tasks have joined.
    ::uwvm2::uwvm::runtime::storage::runtime_registry_type const* compiler_registry{};

    // Disabled emits no bridge symbol, call, load, check or new metadata. The
    // runtime must include enabled instrumentation in its object-cache identity.
    llvm_jit_compilation_mode compilation_mode{llvm_jit_compilation_mode::unspecified};
    bool emit_debug_safe_points{};
    // Owned per-engine compile policy. Null emits no checkpoint IR, allocas,
    // symbol, native call or hot-path check; its complete tuple keys objects.
    ::uwvm2::runtime::checkpoint::compilation_profile::owner checkpoint_profile{};
    // Static generation of this emitted body; the runtime sets replacement +1.
    ::std::uint64_t debug_compiled_function_generation{1u};
    llvm_jit_debug_safe_point_granularity debug_safe_point_granularity{llvm_jit_debug_safe_point_granularity::entry_loop};

    // Optional standalone debug-full routing. Both zero preserves ordinary full IR.
    // The host owns one aligned uintptr_t slot per local function, publishes every
    // nonzero typed entry before execution, and keeps the allocation and all old
    // executable generations alive until execution drains. Replacement requires
    // identical Wasm signature, native calling convention and result-buffer ABI.
    ::std::uintptr_t debug_full_patchable_typed_target_base_address{};
    ::std::size_t debug_full_patchable_typed_target_count{};

    // Parser feature switches used by the authoritative WebAssembly 3.0 validation policy. Null means the historical
    // wasm1p1-compatible default feature set (all release-2.0 groups enabled).
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

    // Emits native unwind metadata for concrete generated functions.
    bool emit_unwind_call_stack_frames{};

    // Internal collector integration. The runtime may enable this only after
    // native readers, static roots, host handles and exception owners are part
    // of one collection protocol. The false default adds no generated code.
    // Include this choice and the root ABI version in every native object key.
    bool emit_precise_gc_root_frames{};

    // PRIVATE host-owned plan for the numeric pending-core experiment.
    // Default null preserves every production mode/ABI/code path.
    ::uwvm2::runtime::exception::pending_experiment::numeric_entry::compile_plan const* pending_numeric_plan{};
    // Private before/after oracle: analyze the same validated complete graph in
    // both cases, but retain conservative presence checks when this is false.
    bool pending_numeric_fold_proven_empty_calls{true};

    // Borrowed until all compilation tasks join. The runtime owns this native
    // target and has independently qualified the matching host Itanium C++ ABI.
    // Null leaves native guest EH unavailable; it never selects a CPU whitelist.
    ::llvm::TargetMachine const* native_exception_target_machine{};

#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    // Recording request only. A real canonical initialized source owner is
    // mandatory; no validation epoch or executable permit is manufactured.
    bool record_native_eh_leaf_observations{};
    native_eh_leaf_observer::source_type::owner native_eh_leaf_source_owner{};
    native_eh_leaf_observer::limits native_eh_leaf_observation_limits{};
    // Internal current traversal, overwritten by the actual all-function entry.
    native_eh_leaf_observer::module_attempt::owner native_eh_leaf_active_attempt{};

#endif
    // Optional per-task module callback used by optimization/linking pipelines.
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
    bool stage_native_eh_private_leaf{}; // Default false; never an executable request.
#endif
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

#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
// Exact friend declaration: only the genuine full-fusion entry mints staging.
inline constexpr full_function_symbol_t compile_all_from_uwvm(
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const&, compile_option&,
    ::uwvm2::validation::error::code_validation_error_impl&, ::std::size_t, compile_task_split_config) UWVM_THROWS;
#endif
namespace details
{
    // Half-open range of local-defined functions assigned to one compile task.
    struct local_function_task_group
    {
        ::std::size_t begin_index{};
        ::std::size_t end_index{};
    };

    // Compile-time adapter from the configured Wasm feature tuple to the parser section-storage types used by validation.
    #include <uwvm2/runtime/compiler/shared/wasm3_runtime_validation_module.h>
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
        // Validation-only bottom value; it has stack arity but no concrete type.
        bool is_unknown{};
        bool is_reference_bottom{};
        // SIZE_MAX denotes an erased funcref; SIZE_MAX - 1 is the nofunc bottom heap type.
        // Any other value indexes this module's type section and proves an exact function signature.
        ::std::size_t exact_function_type_index{(::std::numeric_limits<::std::size_t>::max)()};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type core_type{};
        bool has_core_type{};

        // Monotonic id assigned when the value is produced.
        runtime_virtual_register_id virtual_register_id{invalid_runtime_virtual_register_id};
    };

    using runtime_operand_stack_type = ::uwvm2::utils::container::deque<runtime_operand_stack_storage_t>;

    // Stable virtual-register assignment for one Wasm local slot.
    struct runtime_local_virtual_register_t
    {
        runtime_operand_stack_value_type type{};
        // Compile-time Core 3 heap witness; it never changes the generated local ABI.
        ::std::size_t exact_function_type_index{(::std::numeric_limits<::std::size_t>::max)()};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type core_type{};
        bool has_core_type{};
        runtime_virtual_register_id virtual_register_id{invalid_runtime_virtual_register_id};
    };

    using runtime_local_virtual_register_table_t = ::uwvm2::utils::container::deque<runtime_local_virtual_register_t>;

    // Runtime block tuple represented as a pointer pair into parser/runtime type storage or static inline-result arrays.
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
        // Parameter/start tuple re-established at the beginning of the construct. For a loop this is also its label tuple.
        runtime_block_result_type params{};

        // Result values required at the construct's merge/return point.
        runtime_block_result_type result{};

        // Operand stack height before entering the construct.
        ::std::size_t operand_stack_base{};

        // Construct kind.
        block_type type{};

        // True when the construct was entered from an unreachable/polymorphic region.
        bool polymorphic_base{};

        // Core 3 initialization-stack height at entry, restored by else/end.
        ::std::size_t local_init_checkpoint{};
        ::std::size_t signature_type_index{(::std::numeric_limits<::std::size_t>::max)()};
        ::std::size_t singleton_result_witness{(::std::numeric_limits<::std::size_t>::max)()};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type singleton_result_core_type{};
        bool has_singleton_result_core_type{};
    ::uwvm2::runtime::compiler::shared::wasm_exception_control::handlers exception_handlers{};
        ::std::size_t checkpoint_scope{SIZE_MAX}; // fused observer lexical scope, never a runtime identity

    };

    // Runtime module storage should already be finalized by the parser/initializer.  Violations here mean host/runtime
    // corruption, not guest validation failure, so the JIT fails fast instead of reporting a Wasm error.
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
                                    ::uwvm2::utils::container::vector<tiered_loop_reentry_storage_t>* tiered_loop_reentries_out = nullptr,
                                    bool emit_debug_safe_points = false,
                                    llvm_jit_compilation_mode compilation_mode = llvm_jit_compilation_mode::unspecified,
                                    llvm_jit_debug_safe_point_granularity debug_safe_point_granularity = llvm_jit_debug_safe_point_granularity::entry_loop,
                                    ::std::uintptr_t debug_full_patchable_typed_target_base_address = 0u,
                                    ::std::size_t debug_full_patchable_typed_target_count = 0uz,
                                    ::uwvm2::utils::container::vector<::std::uint_least8_t>* debug_safe_point_bits = nullptr,
                                    bool emit_precise_gc_root_frames = false,
                                    ::uwvm2::runtime::exception::pending_experiment::numeric_entry::compile_plan const* pending_numeric_plan = nullptr,
                                    ::std::uint64_t debug_compiled_function_generation = 1u,
                                    ::uwvm2::runtime::checkpoint::function_plan* checkpoint_plan = nullptr,
                                    ::std::vector<::uwvm2::validation::standard::wasm3::validated_call_dependency>* lazy_call_dependencies = nullptr,
                                    bool* lazy_call_dependencies_overflow = nullptr) UWVM_THROWS
    {
        auto const retain_lazy_dependency{[&](::uwvm2::validation::standard::wasm3::validated_call_dependency dependency)
        {
            if(lazy_call_dependencies == nullptr) { return; }
            // Bounded compiler DATA only. Exhaustion never changes Wasm typing;
            // the admission factory reports resource exhaustion after all bodies.
            constexpr ::std::size_t limit{65536uz};
            if(lazy_call_dependencies->size() >= limit)
            { if(lazy_call_dependencies_overflow != nullptr) { *lazy_call_dependencies_overflow = true; } return; }
            lazy_call_dependencies->push_back(dependency);
        }};
        auto const function_index{local_func_storage.function_index};
        auto const code_begin{local_func_storage.code_begin};
        auto const code_end{local_func_storage.code_end};
        auto const runtime_module_ptr{local_func_storage.runtime_module_ptr};
        if(runtime_module_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
        auto const& curr_module{*runtime_module_ptr};

        // Module function indices include imports first.  This helper validates local defined functions only; imported
        // functions have no Wasm body to validate or emit.
        auto const& importsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::import_section_storage_t>(module_storage.sections)};
        static_assert(importsec.importdesc_count > 0uz);
        auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};
        if(function_index < import_func_count) [[unlikely]]
        {
            // [function body bytes, possibly empty] | code_end
            // [readable only if nonempty           ] | one-past is not dereferenced
            // ^^ code_begin -> err.err_curr: diagnostic copy; begin may equal end.
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
            // [function body bytes, possibly empty] | code_end
            // [readable only if nonempty           ] | one-past is not dereferenced
            // ^^ code_begin -> err.err_curr: diagnostic copy; begin may equal end.
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
        // Imported and local tables share one index space. Core 1.0/MVP encodes call_indirect's table operand as the
        // literal reserved byte 0x00; Reference Types/Core 2.0 encode `tableidx ::= u32`. Opcode validation first decodes
        // that versioned syntax, then applies the enabled multiple-table policy and this combined range.
        auto const all_table_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_table_count + local_table_count)};

        auto const& elemsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::element_section_storage_t>(
                module_storage.sections)};

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

        auto const& datasec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::data_section_storage_t>(module_storage.sections)};
        auto const& datacountsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<validation_module_traits_t::data_count_section_storage_t>(
                module_storage.sections)};

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

        auto const exact_function_type_witness{[&](::uwvm2::parser::wasm::standard::wasm3::type::core_value_type const& core_type)
            constexpr noexcept -> ::std::size_t
        {
            namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
            constexpr auto no_witness{(::std::numeric_limits<::std::size_t>::max)()};
            if(core_type.kind != t::value_kind::reference) { return no_witness; }
            if(core_type.heap.is_defined())
            {
                auto const index{static_cast<::std::size_t>(core_type.heap.code)};
                if(index >= typesec.types.size()) [[unlikely]] { runtime_storage_bug(); }
                return index;
            }
            if(core_type.heap.code == static_cast<::std::int_least64_t>(t::abstract_heap_type::nofunc))
            { return no_witness - 1uz; }
            return no_witness;
        }};

        auto const local_virtual_registers_push_back{
            [&](curr_operand_stack_value_type type, ::std::size_t witness = (::std::numeric_limits<::std::size_t>::max)(),
                ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type core_type = {}, bool has_core_type = false) constexpr noexcept
            { local_virtual_registers.push_back(curr_local_virtual_register_t{
                .type = type, .exact_function_type_index = witness, .core_type = core_type,
                .has_core_type = has_core_type, .virtual_register_id = next_virtual_register_id++}); }};

        // Assign stable virtual registers to parameters first, then declared locals, matching Wasm local-index order.
        auto const owned_signature_count{typesec.owned_signatures.size()};
        if(owned_signature_count != 0uz && owned_signature_count != typesec.types.size()) [[unlikely]] { runtime_storage_bug(); }
        auto const have_rich_signature{owned_signature_count != 0uz};
        auto const curr_function_type_index{static_cast<::std::size_t>(funcsec.funcs.index_unchecked(local_func_idx))};
        if(have_rich_signature && typesec.owned_signatures.index_unchecked(curr_function_type_index).parameters.size() != func_parameter_count_uz)
            [[unlikely]] { runtime_storage_bug(); }
        for(::std::size_t i{}; i != func_parameter_count_uz; ++i)
        {
            // [func_parameter_begin, func_parameter_end) is the validated signature range.
            // [safe                                      ] i < func_parameter_count_uz proves this read.
            //         ^^ func_parameter_begin[i] is borrowed; the pointer never advances here.
            auto const core_type{have_rich_signature ?
                typesec.owned_signatures.index_unchecked(curr_function_type_index).parameters.index_unchecked(i) :
                ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type{}};
            auto const witness{have_rich_signature ? exact_function_type_witness(core_type) :
                (::std::numeric_limits<::std::size_t>::max)()};
            local_virtual_registers_push_back(func_parameter_begin[i], witness, core_type, have_rich_signature);
        }
        for(auto const& local_part: curr_code_locals)
        {
            auto const witness{local_part.has_core_type ? exact_function_type_witness(local_part.core_type) :
                (::std::numeric_limits<::std::size_t>::max)()};
            for(validation_module_traits_t::wasm_u32 i{}; i != local_part.count; ++i)
            { local_virtual_registers_push_back(local_part.type, witness, local_part.core_type, local_part.has_core_type); }
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

        // Parameters always arrive initialized, including non-null references. Declared numeric/vector and
        // nullable-reference locals have a default value; only non-defaultable declared locals need sparse tracking.
        ::uwvm2::validation::standard::wasm3::core3_local_initialization initialized_locals{};
        auto const local_initially_initialized{[&](validation_module_traits_t::wasm_u32 local_index) noexcept
        {
            if(local_index < func_parameter_count_u32) { return true; }
            auto const& declaration{local_virtual_register_from_index(local_index)};
            auto const core_type{declaration.has_core_type ? declaration.core_type :
                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(declaration.type)};
            return ::uwvm2::validation::standard::wasm3::core3_value_is_defaultable(core_type);
        }};

        struct debug_local_initialization_context
        {
            decltype(local_initially_initialized) const* defaults{};
            ::uwvm2::validation::standard::wasm3::core3_local_initialization const* current{};
            ::std::size_t count{};
        };
        debug_local_initialization_context const debug_local_initialization_owner{
            ::std::addressof(local_initially_initialized), ::std::addressof(initialized_locals), local_virtual_registers.size()};
        llvm_jit_debug_local_initialization_query const debug_local_initialization{
            ::std::addressof(debug_local_initialization_owner),
            +[](void const* context, ::std::size_t index, bool& initialized) noexcept -> bool
            {
                if(context == nullptr) { return false; }
                auto const& owner{*static_cast<debug_local_initialization_context const*>(context)};
                if(owner.defaults == nullptr || owner.current == nullptr || index >= owner.count || index > 0xffffffffu)
                { return false; }
                auto const local_index{static_cast<validation_module_traits_t::wasm_u32>(index)};
                initialized = owner.current->is_initialized(local_index, (*owner.defaults)(local_index));
                return true;
            }};
        // [compiler-owned local initialization context] complete fused call
        // [safe                                      ] query only borrows this
        // lexical owner; prepare and every structural/ordinary emitter finish
        // before its validator/local table/current-state references retire.

        auto const allocate_virtual_register{[&]() constexpr noexcept -> curr_runtime_virtual_register_id
                                             {
                                                 auto const virtual_register_id{next_virtual_register_id};
                                                 if(virtual_register_id == invalid_runtime_virtual_register_id) [[unlikely]] { runtime_storage_bug(); }
                                                 ++next_virtual_register_id;
                                                 if(next_virtual_register_id == invalid_runtime_virtual_register_id) [[unlikely]] { runtime_storage_bug(); }
                                                 return virtual_register_id;
                                             }};

        auto const operand_stack_push{
            [&](curr_operand_stack_value_type type, bool is_unknown = false,
                ::std::size_t exact_function_type_index = (::std::numeric_limits<::std::size_t>::max)(),
                ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type core_type = {}, bool has_core_type = false) constexpr noexcept
            { operand_stack.push_back(runtime_operand_stack_storage_t{.type = type, .is_unknown = is_unknown,
                .exact_function_type_index = exact_function_type_index, .core_type = core_type,
                .has_core_type = has_core_type, .virtual_register_id = allocate_virtual_register()}); }};

        auto const operand_stack_push_function_results{[&](auto const& function_type, ::std::size_t function_type_index)
            constexpr noexcept
        {
            auto const result_count{function_type.result.begin == function_type.result.end ? 0uz :
                static_cast<::std::size_t>(function_type.result.end - function_type.result.begin)};
            if(function_type_index >= typesec.types.size()) [[unlikely]] { runtime_storage_bug(); }
            auto const have_rich_signature{typesec.owned_signatures.size() == typesec.types.size() &&
                !typesec.owned_signatures.empty()};
            if(have_rich_signature && typesec.owned_signatures.index_unchecked(function_type_index).results.size() != result_count)
                [[unlikely]] { runtime_storage_bug(); }
            for(::std::size_t i{}; i != result_count; ++i)
            {
                // [result.begin, result.end) is the validated function result range.
                // [safe                    ] i < result_count proves this read.
                //         ^^ result.begin[i] is borrowed; the pointer remains unchanged.
                auto const carrier{function_type.result.begin[i]};
                auto const core_type{have_rich_signature ?
                    typesec.owned_signatures.index_unchecked(function_type_index).results.index_unchecked(i) :
                    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type{}};
                auto const witness{carrier == curr_operand_stack_value_type::funcref && have_rich_signature ?
                    exact_function_type_witness(core_type) : (::std::numeric_limits<::std::size_t>::max)()};
                operand_stack_push(carrier, false, witness, core_type, have_rich_signature);
            }
        }};

        auto const synthetic_function_type_index_from_pointer{
            [&](::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* type_ptr) constexpr noexcept
                -> ::std::size_t
            {
                auto const type_begin{typesec.types.cbegin()};
                auto const type_end{typesec.types.cend()};
                if(type_ptr == nullptr || type_begin == type_end) [[unlikely]] { runtime_storage_bug(); }
                auto const ptr_addr{reinterpret_cast<::std::uintptr_t>(type_ptr)};
                auto const begin_addr{reinterpret_cast<::std::uintptr_t>(type_begin)};
                auto const end_addr{reinterpret_cast<::std::uintptr_t>(type_end)};
                if(ptr_addr < begin_addr || ptr_addr >= end_addr) [[unlikely]] { runtime_storage_bug(); }
                auto const byte_offset{ptr_addr - begin_addr};
                if(byte_offset % sizeof(*type_begin) != 0uz) [[unlikely]] { runtime_storage_bug(); }
                // [type_begin, type_end) is the synthetic section's retained allocation.
                // [safe                  ] aligned byte_offset names one live record.
                //         ^^ type_ptr is only identified; no pointer is advanced or dereferenced here.
                return byte_offset / sizeof(*type_begin);
            }};

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
            bool is_unknown{};
            bool is_reference_bottom{};
            ::std::size_t exact_function_type_index{(::std::numeric_limits<::std::size_t>::max)()};
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type core_type{};
            bool has_core_type{};
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
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                                                return {.from_stack = true, .type = operand.type, .is_unknown = operand.is_unknown,
                                                    .is_reference_bottom = operand.is_reference_bottom,
                                                    .exact_function_type_index = operand.exact_function_type_index,
                                                    .core_type = operand.core_type, .has_core_type = operand.has_core_type};
                                            }};

        auto const try_peek_concrete_operand{[&]() constexpr noexcept -> concrete_operand_t
                                             {
                                                 if(concrete_operand_count() == 0uz) { return {}; }
                                                 return {.from_stack = true, .type = operand_stack.back().type, .is_unknown = operand_stack.back().is_unknown,
                                                     .is_reference_bottom = operand_stack.back().is_reference_bottom,
                                                     .exact_function_type_index = operand_stack.back().exact_function_type_index,
                                                     .core_type = operand_stack.back().core_type,
                                                     .has_core_type = operand_stack.back().has_core_type};
                                             }};

        llvm_jit_checkpoint_observer_control_map checkpoint_observer_controls{checkpoint_plan};
        // Function block (label/result tuple is the function result tuple).
        control_flow_stack.push_back({
            .params = {},
            .result = {.begin = curr_func_type.result.begin, .end = curr_func_type.result.end},
            .operand_stack_base = 0uz,
            .type = block_type::function,
            .polymorphic_base = false,
            .local_init_checkpoint = initialized_locals.checkpoint(),
            .signature_type_index = curr_function_type_index,
            .checkpoint_scope = 0u,
        });

        auto const operand_stack_push_types{
            [&](runtime_block_result_type types, ::std::size_t type_index,
                bool results, ::std::size_t singleton_witness = (::std::numeric_limits<::std::size_t>::max)(),
                ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type singleton_core_type = {},
                bool has_singleton_core_type = false) constexpr noexcept
            {
                auto const count{types.begin == types.end ? 0uz : static_cast<::std::size_t>(types.end - types.begin)};
                auto const rich_available{type_index < typesec.owned_signatures.size() &&
                    typesec.owned_signatures.size() == typesec.types.size()};
                if(rich_available)
                {
                    auto const& signature{typesec.owned_signatures.index_unchecked(type_index)};
                    auto const rich_count{results ? signature.results.size() : signature.parameters.size()};
                    if(rich_count != count) [[unlikely]] { runtime_storage_bug(); }
                }
                for(::std::size_t i{}; i != count; ++i)
                {
                    // [types.begin, types.end) is the validated block tuple.
                    // [safe                  ] i < count proves this read.
                    //         ^^ types.begin[i] is borrowed; neither endpoint is advanced.
                    auto const carrier{types.begin[i]};
                    auto witness{(::std::numeric_limits<::std::size_t>::max)()};
                    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type core_type{};
                    bool has_core_type{};
                    if(carrier == curr_operand_stack_value_type::funcref)
                    {
                        if(rich_available)
                        {
                            auto const& signature{typesec.owned_signatures.index_unchecked(type_index)};
                            core_type = results ? signature.results.index_unchecked(i) : signature.parameters.index_unchecked(i);
                            witness = exact_function_type_witness(core_type);
                            has_core_type = true;
                        }
                        else if(results && count == 1uz)
                        {
                            witness = singleton_witness;
                            core_type = singleton_core_type;
                            has_core_type = has_singleton_core_type;
                        }
                    }
                    else if(rich_available)
                    {
                        auto const& signature{typesec.owned_signatures.index_unchecked(type_index)};
                        core_type = results ? signature.results.index_unchecked(i) : signature.parameters.index_unchecked(i);
                        has_core_type = true;
                    }
                    else if(results && count == 1uz)
                    { core_type = singleton_core_type; has_core_type = has_singleton_core_type; }
                    operand_stack_push(carrier, false, witness, core_type, has_core_type);
                }
            }};

        struct core3_block_type_at_t
        {
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type type{};
            bool has_type{};
        };
        auto const block_core_type_at{
            [&](runtime_block_result_type types, ::std::size_t signature_type_index, bool results,
                bool has_singleton_result_core_type,
                ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type singleton_result_core_type,
                ::std::size_t index) constexpr noexcept -> core3_block_type_at_t
            {
                auto const count{types.begin == types.end ? 0uz : static_cast<::std::size_t>(types.end - types.begin)};
                if(index >= count) [[unlikely]] { runtime_storage_bug(); }
                if(signature_type_index < typesec.owned_signatures.size() &&
                   typesec.owned_signatures.size() == typesec.types.size())
                {
                    auto const& signature{typesec.owned_signatures.index_unchecked(signature_type_index)};
                    auto const& tuple{results ? signature.results : signature.parameters};
                    if(tuple.size() != count) [[unlikely]] { runtime_storage_bug(); }
                    // [tuple.begin, tuple.end) is parser-owned for the full JIT translation.
                    // [safe                 ] index < count proves this read.
                    //         ^^ index_unchecked(index) borrows one type; no cursor advances.
                    return {tuple.index_unchecked(index), true};
                }
                if(results && has_singleton_result_core_type && count == 1uz)
                { return {singleton_result_core_type, true}; }
                return {};
            }};
        // Validated Core 3 subtype metadata distinguishes aggregate heaps with identical ABI placeholders.
        auto const runtime_core3_value_type_matches{[&](auto actual, auto expected, auto const& signatures) constexpr noexcept
        {
            // typesec is the actual retained parser section for this fused compile.
            // Its immutable context borrow survives this call; no guest cursor moves.
            return ::uwvm2::validation::standard::wasm3::core3_value_type_matches_with_context(
                actual, expected, signatures, ::std::addressof(typesec.core3_context));
        }};
        auto const operand_core_type{[&](auto const& actual) constexpr noexcept
            -> ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type
        {
            namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
            if(actual.has_core_type || actual.is_reference_bottom)
            { return ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual); }
            if(actual.type == curr_operand_stack_value_type::funcref)
            {
                if(actual.exact_function_type_index < typesec.types.size())
                { return {t::value_kind::reference, {static_cast<::std::int_least64_t>(actual.exact_function_type_index)}, true}; }
                if(actual.exact_function_type_index == (::std::numeric_limits<::std::size_t>::max)() - 1uz)
                { return {t::value_kind::reference,
                    {static_cast<::std::int_least64_t>(t::abstract_heap_type::nofunc)}, true}; }
            }
            return ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(actual.type);
        }};
        auto const core3_value_matches{[&](auto const& actual, curr_operand_stack_value_type expected_carrier,
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type expected_core_type,
            bool has_expected_core_type) constexpr noexcept -> bool
        {
            if(actual.is_unknown) { return true; }
            if(!has_expected_core_type && !actual.has_core_type && !actual.is_reference_bottom &&
               actual.exact_function_type_index == (::std::numeric_limits<::std::size_t>::max)())
            { return ::uwvm2::validation::standard::wasm3::reference_carrier_matches(actual, expected_carrier); }
            auto const expected{has_expected_core_type ? expected_core_type :
                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(expected_carrier)};
            return runtime_core3_value_type_matches(
                operand_core_type(actual), expected, typesec.owned_signatures);
        }};
        auto const block_value_matches{[&](auto const& actual, runtime_block_result_type types,
            ::std::size_t signature_type_index, bool results, bool has_singleton_result_core_type,
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type singleton_result_core_type,
            ::std::size_t index) constexpr noexcept -> bool
        {
            auto const expected{block_core_type_at(types, signature_type_index, results,
                has_singleton_result_core_type, singleton_result_core_type, index)};
            // [types.begin, types.end) is the retained flat signature range.
            // [safe                   ] block_core_type_at proved index < count.
            //         ^^ begin[index] reads a carrier; no pointer advances.
            return core3_value_matches(actual, types.begin[index], expected.type, expected.has_type);
        }};

        auto const callee_argument_matches{[&](auto const& actual,
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& callee_type,
            ::std::size_t callee_type_index, ::std::size_t parameter_index) constexpr noexcept -> bool
        {
            auto const count{callee_type.parameter.begin == callee_type.parameter.end ? 0uz :
                static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
            if(parameter_index >= count) [[unlikely]] { runtime_storage_bug(); }
            auto const rich{callee_type_index < typesec.owned_signatures.size() &&
                typesec.owned_signatures.size() == typesec.types.size()};
            if(rich && typesec.owned_signatures.index_unchecked(callee_type_index).parameters.size() != count)
                [[unlikely]] { runtime_storage_bug(); }
            auto const expected{rich ? typesec.owned_signatures.index_unchecked(callee_type_index).parameters.index_unchecked(parameter_index) :
                ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type{}};
            // [parameter.begin, parameter.end) belongs to the retained type-section record.
            // [safe                           ] parameter_index < count proves this read.
            //         ^^ begin[parameter_index] reads one carrier; no cursor advances.
            return core3_value_matches(actual, callee_type.parameter.begin[parameter_index], expected, rich);
        }};

        auto const validate_rich_tail_results{[&](::std::byte const* op_begin,
            ::uwvm2::utils::container::u8string_view op_name,
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& callee_type,
            ::std::size_t callee_type_index) constexpr UWVM_THROWS
        {
            ::uwvm2::validation::standard::wasm3::validate_tail_call_results(
                control_flow_stack.index_unchecked(0u).result, callee_type.result, op_begin, op_name, err);
            if(typesec.owned_signatures.size() != typesec.types.size() || typesec.owned_signatures.empty()) { return; }
            if(callee_type_index >= typesec.owned_signatures.size() ||
               curr_function_type_index >= typesec.owned_signatures.size()) [[unlikely]] { runtime_storage_bug(); }
            auto const& callee_results{typesec.owned_signatures.index_unchecked(callee_type_index).results};
            auto const& caller_results{typesec.owned_signatures.index_unchecked(curr_function_type_index).results};
            if(callee_results.size() != caller_results.size()) [[unlikely]] { runtime_storage_bug(); }
            for(::std::size_t i{}; i != callee_results.size(); ++i)
            {
                if(runtime_core3_value_type_matches(
                    callee_results.index_unchecked(i), caller_results.index_unchecked(i), typesec.owned_signatures)) { continue; }
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin; // Borrow the checked opcode; no input cursor changes.
                err.err_selectable.br_value_type_mismatch.op_code_name = op_name;
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(
                    control_flow_stack.index_unchecked(0u).result.begin[i]);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(callee_type.result.begin[i]);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }};

        // Validate and consume a complete type tuple from the current frame. In polymorphic code only concrete operands
        // above the frame base are consumed; missing operands are supplied abstractly by the validation rules.
        auto const operand_stack_pop_expected_types{
            [&](::std::byte const* op_begin,
                ::uwvm2::utils::container::u8string_view op_name,
                runtime_block_result_type expected, ::std::size_t signature_type_index) constexpr UWVM_THROWS
            {
                auto const expected_count{get_runtime_block_result_count(expected)};
                auto const available_count{concrete_operand_count()};
                if(!is_polymorphic && available_count < expected_count) [[unlikely]]
                {
                    report_operand_stack_underflow(op_begin, op_name, expected_count);
                }

                auto const concrete_to_check{available_count < expected_count ? available_count : expected_count};
                auto const stack_size{operand_stack.size()};
                for(::std::size_t i{}; i != concrete_to_check; ++i)
                {
                    auto const expected_type{expected.begin[expected_count - 1uz - i]};
                    auto const& actual_operand{operand_stack[stack_size - 1uz - i]};
                    auto const actual_type{actual_operand.type};
                    if(!block_value_matches(actual_operand, expected, signature_type_index, false,
                        false, {}, expected_count - 1uz - i)) [[unlikely]]
                    {
                        // [caller-saved opcode/prefix] immediate bytes ... | code_end
                        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                        err.err_curr = op_begin;
                        err.err_selectable.br_value_type_mismatch.op_code_name = op_name;
                        err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(expected_type);
                        err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(actual_type);
                        err.err_code = code_validation_error_code::br_value_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                }

                operand_stack_pop_n(concrete_to_check);
            }};

        auto const enter_control_frame{
            [&](::std::byte const* op_begin,
                ::uwvm2::utils::container::u8string_view op_name,
                block_type type,
                runtime_block_signature_type signature) constexpr UWVM_THROWS
            {
                operand_stack_pop_expected_types(op_begin, op_name, signature.params, signature.type_index);
                auto const outer_stack_height{operand_stack.size()};
                control_flow_stack.push_back({.params = signature.params,
                                              .result = signature.results,
                                              .operand_stack_base = outer_stack_height,
                                              .type = type,
                                              .polymorphic_base = is_polymorphic,
                                              .local_init_checkpoint = initialized_locals.checkpoint(),
                                              .signature_type_index = signature.type_index,
                                              .singleton_result_witness = signature.singleton_result_witness,
                                              .singleton_result_core_type = signature.singleton_result_core_type,
                                              .has_singleton_result_core_type = signature.has_singleton_result_core_type,
                                              .checkpoint_scope = checkpoint_observer_controls.selected() ?
                                                  checkpoint_observer_controls.begin(static_cast<::std::size_t>(op_begin-code_begin)) : SIZE_MAX});
                // [code_begin ... dispatch-proven op_begin ...] | code_end
                // [safe same immutable expression allocation  ] | one-past
                // Only the bounded lexical offset is copied; no cursor moves.
                operand_stack_push_types(signature.params, signature.type_index, false);
                is_polymorphic = false;
            }};

        // Start parsing the code body.
        auto code_curr{code_begin};
        parser_feature_parameter_t const default_validator_feature_parameter{};
        auto const& effective_validator_feature_parameter{
            validator_feature_parameter == nullptr ? default_validator_feature_parameter : *validator_feature_parameter};
        [[maybe_unused]] auto const& wasm1p1_para{
            ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(effective_validator_feature_parameter)};

        runtime_local_func_llvm_jit_emit_state_t llvm_jit_emit_state{};
        // The entry layout uses the same exact Core3 local/signature metadata
        // before any instruction consumes operands. It is never reconstructed
        // from the LLVM physical carrier or a second bytecode pass.
        if(checkpoint_plan != nullptr)
        {
            namespace checkpoint = ::uwvm2::runtime::checkpoint;
            if(!checkpoint_plan->profile || !emit_debug_safe_points ||
               compilation_mode != llvm_jit_compilation_mode::full || debug_compiled_function_generation == 0u)
            { runtime_storage_bug(); }
            checkpoint_plan->module = local_func_storage.module_id;
            checkpoint_plan->function = local_func_storage.function_index;
            checkpoint_plan->function_generation = debug_compiled_function_generation;
            // [code_begin ... expression bytes ...] | code_end
            // [same checked source allocation      ] | one-past
            // Both endpoints were checked by runtime storage construction;
            // only this bounded difference is copied, neither pointer moves.
            auto const begin_address{reinterpret_cast<::std::uintptr_t>(code_begin)};
            auto const end_address{reinterpret_cast<::std::uintptr_t>(code_end)};
            if(begin_address == 0u || end_address <= begin_address || end_address - begin_address >
               static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()))
            { checkpoint_plan->compiler_failure = checkpoint::status::invalid_layout; }
            else { checkpoint_plan->expression_bytes = end_address - begin_address; }
            if(local_virtual_registers.size() > checkpoint_plan->profile->limits().slots_per_frame ||
               !(checkpoint_plan->profile->purpose() == checkpoint::compilation_purpose::observe_values ?
                 checkpoint::observer_workspace_fits(local_virtual_registers.size(), local_virtual_registers.size()) :
                 checkpoint::native_workspace_fits(local_virtual_registers.size(), local_virtual_registers.size())))
            { checkpoint_plan->producer_availability = checkpoint::status::quota_exceeded; }
            if(checkpoint_plan->compiler_failure == checkpoint::status::ok && checkpoint_plan->producer_availability == checkpoint::status::ok)
            {
                checkpoint::safepoint_layout entry{};
                entry.identifier = 1u; entry.local_count = local_virtual_registers.size();
                for(::std::size_t i{}; i != local_virtual_registers.size(); ++i)
                {
                    // [same owned exact local declarations ... i ...] locals_end
                    // [safe                                        ] i<size
                    // before deque indexing; no byte cursor is advanced.
                    auto const& local{local_virtual_registers[i]};
                    auto const type{local.has_core_type ? local.core_type :
                        ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(local.type)};
                    entry.slots.push_back({type, local_initially_initialized(static_cast<validation_module_traits_t::wasm_u32>(i))});
                }
                checkpoint::control_layout function{};
                function.kind = checkpoint::control_kind::function;
                if(checkpoint_plan->expression_bytes != 0u) { function.end_offset = checkpoint_plan->expression_bytes - 1u; }
                auto const have_rich{typesec.owned_signatures.size() == typesec.types.size() && !typesec.owned_signatures.empty()};
                auto const result_count{curr_func_type.result.begin == curr_func_type.result.end ? 0u :
                    static_cast<::std::size_t>(curr_func_type.result.end - curr_func_type.result.begin)};
                if(have_rich && typesec.owned_signatures.index_unchecked(curr_function_type_index).results.size() != result_count)
                { runtime_storage_bug(); }
                if(result_count > checkpoint_plan->profile->limits().slots_per_frame)
                { checkpoint_plan->producer_availability = checkpoint::status::quota_exceeded; }
                for(::std::size_t i{}; checkpoint_plan->compiler_failure == checkpoint::status::ok &&
                    checkpoint_plan->producer_availability == checkpoint::status::ok && i != result_count; ++i)
                {
                    // [actual finalized result tuple ...] result_end
                    // [safe                             ] i<tuple count;
                    // original declaration retained, no SSA subtype substitution.
                    function.declared_results.push_back(have_rich ?
                        typesec.owned_signatures.index_unchecked(curr_function_type_index).results.index_unchecked(i) :
                        ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(curr_func_type.result.begin[i]));
                }
                entry.controls.push_back(::std::move(function));
                if(checkpoint_plan->producer_availability == checkpoint::status::ok)
                { checkpoint_plan->sites.push_back(::std::move(entry)); }
            }
        }
        if(checkpoint_observer_controls.selected())
        {
            auto const function_scope{checkpoint_observer_controls.begin(0u)};
            if(function_scope != 0u && checkpoint_plan->producer_availability == ::uwvm2::runtime::checkpoint::status::ok)
            { runtime_storage_bug(); }
            control_flow_stack.front_unchecked().checkpoint_scope = function_scope;
        }
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
                                                                                     emit_unwind_call_stack_frames,
                                                                                     emit_debug_safe_points,
                                                                                     compilation_mode,
                                                                                     debug_safe_point_granularity,
                                                                                     debug_full_patchable_typed_target_base_address,
                                                                                     debug_full_patchable_typed_target_count,
                                                                                     debug_safe_point_bits,
                                                                                     emit_precise_gc_root_frames,
                                                                                     pending_numeric_plan,
                                                                                     debug_compiled_function_generation,
                                                                                     debug_local_initialization,
                                                                                     checkpoint_plan)};
        // The nonnull sink is owned only by checked_lazy_ir_plan::admit. This
        // adds no full options/local-func ABI field or normal full/ROS IR.
        if(emit_llvm_jit_active)
        { llvm_jit_emit_state.stage_retained_unwind_import_routes = lazy_call_dependencies != nullptr; }

        if(emit_llvm_jit_active && checkpoint_observer_controls.selected())
        { llvm_jit_emit_state.checkpoint_observer_controls = ::std::addressof(checkpoint_observer_controls); }

        if(emitted_llvm_jit_ir_storage != nullptr && !emit_llvm_jit_active)
        {
            emitted_llvm_jit_ir_storage->note_first_decline(
                llvm_jit_compiler_decline_stage::function_prepare, local_func_storage.function_index);
        }
        if(checkpoint_plan != nullptr && !emit_llvm_jit_active && emitted_llvm_jit_ir_storage != nullptr)
        {
            // A partial native entry/cleanup/packet is not executable policy.
            // Preserve authoritative validation, but reject the whole emitted
            // fragment before any empty/partial module can appear publishable.
            emitted_llvm_jit_ir_storage->discard_emission_preserving_first_decline();
            if(checkpoint_plan->compiler_failure == ::uwvm2::runtime::checkpoint::status::ok)
            { checkpoint_plan->compiler_failure = ::uwvm2::runtime::checkpoint::status::invalid_plan; }
        }

        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        using wasm1p1_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_basic;
        using wasm1p1_numeric_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_numeric;

        // The runtime module retains the parser's recursive-type requirement. Check it
        // in this validation/emission pass before any instruction can publish LLVM IR.
        // code_begin is a borrowed diagnostic address inside the validated body span;
        // this policy call neither advances nor dereferences the code cursor.
        // [body bytes ...] code_end
        //  ^^ code_begin: safe to retain, including an empty body at code_end.
        ::uwvm2::validation::standard::wasm3::require_gc_recursive_type_policy(
            !wasm1p1_para.disable_gc, typesec.requires_gc, code_begin, err);
        ::uwvm2::validation::standard::wasm3::require_function_declaration_policy(curr_func_type, curr_code_locals,
            curr_module.type_section_storage.requires_function_references, wasm1p1_para, code_begin, err,
        curr_module.table_declarations_require_function_references, curr_module.global_declarations_require_function_references,
        curr_module.element_declarations_require_function_references, curr_module.tag_section_present || !curr_module.imported_tag_vec_storage.empty(),
            ::std::addressof(typesec.core3_context), typesec.types.size(),
            curr_module.table_declarations_require_gc, curr_module.global_declarations_require_gc, curr_module.element_declarations_require_gc,
            runtime_exception_declaration_requirements(curr_module),
            {.simd = curr_module.type_section_storage.requires_simd,
             .reference_types = curr_module.type_section_storage.requires_reference_types,
             .multi_value = curr_module.type_section_storage.requires_multi_value},
            runtime_storage_declaration_requirements(curr_module), runtime_address_declaration_requirements(curr_module),
            runtime_constant_declaration_requirements(curr_module));
        auto const wasm2_feature_enabled{
            [&](::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind feature) constexpr noexcept
            {
                return ::uwvm2::parser::wasm::standard::wasm2::features::feature_enabled(wasm1p1_para, feature);
            }};

        auto const opcode_byte{[](wasm1p1_code opcode) constexpr noexcept -> validation_module_traits_t::wasm_u32
                               { return static_cast<validation_module_traits_t::wasm_u32>(static_cast<::std::uint_least8_t>(opcode)); }};

        auto const fail_wasm1p1_feature_required{
            [&](::std::byte const* op_begin,
                validation_module_traits_t::wasm_u32 value,
                ::uwvm2::parser::wasm::base::wasm1p1_feature_kind feature,
                ::uwvm2::parser::wasm::base::wasm1p1_error_subject subject) constexpr UWVM_THROWS
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.wasm1p1_feature_required.value = value;
                err.err_selectable.wasm1p1_feature_required.feature = feature;
                err.err_selectable.wasm1p1_feature_required.subject = subject;
                err.err_code = code_validation_error_code::wasm1p1_feature_required;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }};

        auto const fail_wasm2_feature_required{
            [&](::std::byte const* op_begin,
                validation_module_traits_t::wasm_u32 value,
                ::uwvm2::parser::wasm::base::wasm2_feature_kind feature,
                ::uwvm2::parser::wasm::base::wasm2_error_subject subject) constexpr UWVM_THROWS
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.wasm2_feature_required.value = value;
                err.err_selectable.wasm2_feature_required.feature = feature;
                err.err_selectable.wasm2_feature_required.subject = subject;
                err.err_code = code_validation_error_code::wasm2_feature_required;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }};

        auto const fail_invalid_immediate{
            [&](::std::byte const* op_begin,
                ::uwvm2::utils::container::u8string_view op_name,
                ::fast_io::parse_code pc = ::fast_io::parse_code::invalid) constexpr UWVM_THROWS
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.invalid_const_immediate.op_code_name = op_name;
                err.err_code = code_validation_error_code::invalid_const_immediate;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(pc);
            }};

        auto const check_data_index{
            [&](::std::byte const* op_begin, validation_module_traits_t::wasm_u32 data_index) constexpr UWVM_THROWS
            {
                if(!datacountsec.present || data_index >= datacountsec.count || static_cast<::std::size_t>(data_index) >= datasec.datas.size()) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.illegal_data_index.data_index = data_index;
                    err.err_selectable.illegal_data_index.all_data_count = datacountsec.present ? datacountsec.count : 0u;
                    err.err_code = code_validation_error_code::illegal_data_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }};

        auto const check_element_index{
            [&](::std::byte const* op_begin, validation_module_traits_t::wasm_u32 element_index) constexpr UWVM_THROWS
            {
                auto const all_element_count{static_cast<validation_module_traits_t::wasm_u32>(elemsec.elems.size())};
                if(element_index >= all_element_count) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.illegal_element_index.element_index = element_index;
                    err.err_selectable.illegal_element_index.all_element_count = all_element_count;
                    err.err_code = code_validation_error_code::illegal_element_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }};

        auto const check_table_index{
            [&](::std::byte const* op_begin,
                validation_module_traits_t::wasm_u32 table_index,
                validation_module_traits_t::wasm_u32 opcode) constexpr UWVM_THROWS
            {
                if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::multiple_tables) && table_index != 0u)
                    [[unlikely]]
                {
                    fail_wasm2_feature_required(op_begin,
                                                opcode,
                                                ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables,
                                                ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
                }
                if(table_index >= all_table_count) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.illegal_table_index.table_index = table_index;
                    err.err_selectable.illegal_table_index.all_table_count = all_table_count;
                    err.err_code = code_validation_error_code::illegal_table_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }};

        auto const get_table_value_type{
            [&](validation_module_traits_t::wasm_u32 table_index) constexpr noexcept -> curr_operand_stack_value_type
            {
                if(table_index < imported_table_count)
                {
                    auto const imported_table_ptr{imported_tables.index_unchecked(table_index)};
                    return static_cast<curr_operand_stack_value_type>(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(imported_table_ptr->imports.storage.table.reftype));
                }

                auto const local_table_index{table_index - imported_table_count};
                return static_cast<curr_operand_stack_value_type>(
                    ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(tablesec.tables.index_unchecked(local_table_index).reftype));
            }};

        auto const get_table_type_witness{
            [&](validation_module_traits_t::wasm_u32 table_index) constexpr noexcept -> ::std::size_t
            {
                // All callers first check table_index against the combined imported/local table count.
                if(table_index < imported_table_count)
                {
                    auto const imported_table_ptr{imported_tables.index_unchecked(table_index)};
                    if(imported_table_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
                    auto const& table{imported_table_ptr->imports.storage.table};
                    return table.has_core_type ? exact_function_type_witness(table.core_type) :
                        (::std::numeric_limits<::std::size_t>::max)();
                }
                auto const local_table_index{table_index - imported_table_count};
                auto const& table{tablesec.tables.index_unchecked(local_table_index)};
                return table.has_core_type ? exact_function_type_witness(table.core_type) :
                    (::std::numeric_limits<::std::size_t>::max)();
            }};

        auto const get_table_core_type{
            [&](validation_module_traits_t::wasm_u32 table_index) constexpr noexcept -> core3_block_type_at_t
            {
                // The instruction checks table_index against the combined table count before this lookup.
                if(table_index < imported_table_count)
                {
                    auto const imported_table_ptr{imported_tables.index_unchecked(table_index)};
                    if(imported_table_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
                    auto const& table{imported_table_ptr->imports.storage.table};
                    return {table.core_type, table.has_core_type};
                }
                auto const local_table_index{table_index - imported_table_count};
                auto const& table{tablesec.tables.index_unchecked(local_table_index)};
                return {table.core_type, table.has_core_type};
            }};

        auto const check_ref_func_index{
            [&](::std::byte const* op_begin, validation_module_traits_t::wasm_u32 function_index) constexpr UWVM_THROWS
            {
                auto const all_function_size{static_cast<::std::size_t>(import_func_count + local_func_count)};
                if(static_cast<::std::size_t>(function_index) >= all_function_size) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.invalid_function_index.function_index = function_index;
                    err.err_selectable.invalid_function_index.all_function_size = all_function_size;
                    err.err_code = code_validation_error_code::invalid_function_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                auto const runtime_module_ptr{local_func_storage.runtime_module_ptr};
                if(runtime_module_ptr == nullptr) [[unlikely]] { runtime_storage_bug(); }
                bool declared{};
                for(auto const declared_function_index: runtime_module_ptr->declared_ref_funcidx_vec_storage)
                {
                    if(declared_function_index == function_index)
                    {
                        declared = true;
                        break;
                    }
                }
                if(!declared) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.wasm1p1_undeclared_ref_func.function_index = function_index;
                    err.err_code = code_validation_error_code::wasm1p1_undeclared_ref_func;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
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

                // op_name LEB bytes ... end
                // [safe parsed bytes    ] unsafe (could be end)
                //                       ^^ next: successful scanner returns within this same slice.
                // [bounded decoded immediate] next bytes ... | end
                // [safe consumed bytes]       | one-past is never dereferenced here
                // ^^ curr: parse_by_scan succeeded and returned next inside [old curr, end].
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
                // op_name immediate-byte ... end
                // [safe byte                ] unsafe (could be end)
                // ^^ curr: the equality check above proved one complete byte.
                ++curr;
                // op_name immediate-byte ... end
                // [safe byte                ] unsafe (could be end)
                //                            ^^ curr may be one-past and is not read here.
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
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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

        // Internal structured-control validation deliberately repeats the authoritative Wasm2 blocktype gate ordering.
        // The emitter helper resolves already decoded metadata without rereading body bytes. This wrapper owns
        // validation diagnostics and feature policy so internal-validator fuzzing does not depend on a standard prepass.
        auto const parse_validation_block_signature{
            [&](::std::byte const* op_begin, runtime_block_signature_type& block_signature) constexpr UWVM_THROWS
            {
                if(code_curr == code_end) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_code = code_validation_error_code::missing_block_type;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
                }

                // [control opcode][valtype or s33 index ... code_end)
                // [safe          ] nonempty check above proves the prefix readable.
                if(::uwvm2::validation::standard::wasm3::is_core3_extended_block_reference_prefix(
                    ::std::to_integer<unsigned>(*code_curr)))
                {
                    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type exact_type{};
                    auto const carrier{::uwvm2::validation::standard::wasm3::read_core3_value_carrier(
                        code_curr, code_end, !wasm1p1_para.disable_function_references, op_begin, err,
                        typesec.types.size(), ::std::addressof(exact_type), typesec.core3_context,
                        !wasm1p1_para.disable_gc, !wasm1p1_para.disable_exceptions)};
                    // [opcode][checked valtype] next ... code_end
                    // [safe                  ] unsafe (could be code_end)
                    //                          ^^ code_curr after the transactional decoder.
                    // The Core 3 decoder gates the actual heap. Its 0x70
                    // carrier also represents GC objects, so a second legacy
                    // funcref feature check would reject valid GC blocktypes.
                    // The exact first-decode result retains heap/nullability; runtime metadata construction does
                    // not scan that checked valtype again. Any disagreement is inconsistent initialized storage.
                    if(!resolve_decoded_reference_block_signature(carrier, exact_type, curr_module, block_signature))
                    { runtime_storage_bug(); }
                    return;
                }

                auto const blocktype_begin{code_curr};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 blocktype{};
                auto const [blocktype_next,
                            blocktype_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                   ::fast_io::mnp::leb128_get(blocktype))};
                if(blocktype_err != ::fast_io::parse_code::ok) [[unlikely]]
                {
                    fail_invalid_immediate(op_begin, u8"blocktype", blocktype_err);
                }
                // control_op blocktype ... code_end
                // [       safe       ] unsafe (could be code_end)
                //         ^^ code_curr: successful bounded LEB scan proved blocktype_next.
                code_curr = reinterpret_cast<::std::byte const*>(blocktype_next);

                // control_op blocktype ...
                // [       safe       ] unsafe (could be the section_end)
                //                      ^^ code_curr

                // A decode failure leaves code_curr at blocktype_begin; the checks below run after a complete immediate
                // was committed and therefore report any grammar/feature failure with code_curr past that immediate.

                auto const fail_illegal_blocktype{[&]() constexpr UWVM_THROWS
                                                  {
                                                      ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte first_byte{};
                                                      ::std::memcpy(::std::addressof(first_byte), blocktype_begin, sizeof(first_byte));
#if CHAR_BIT > 8
                                                      first_byte = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                                                          static_cast<::std::uint_least8_t>(first_byte) & 0xFFu);
#endif
                                                      // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                                      // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                                      // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                                      err.err_curr = op_begin;
                                                      err.err_selectable.u8 = first_byte;
                                                      err.err_code = code_validation_error_code::illegal_block_type;
                                                      ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                                  }};

                auto const blocktype_encoded_size{static_cast<::std::size_t>(code_curr - blocktype_begin)};
                if(blocktype_encoded_size > 5uz || (blocktype < 0 && blocktype_encoded_size != 1uz)) [[unlikely]]
                {
                    fail_illegal_blocktype();
                }

                switch(blocktype)
                {
                    case -64:
                    case -1:
                    case -2:
                    case -3:
                    case -4:
                    {
                        break;
                    }
                    case -5:
                    {
                        ensure_wasm1p1_value_type_enabled(op_begin,
                                                          runtime_operand_stack_value_type::v128,
                                                          ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                        break;
                    }
                    case -16:
                    {
                        ensure_wasm1p1_value_type_enabled(op_begin,
                                                          runtime_operand_stack_value_type::funcref,
                                                          ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                        break;
                    }
                    case -17:
                    {
                        ensure_wasm1p1_value_type_enabled(op_begin,
                                                          runtime_operand_stack_value_type::externref,
                                                          ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                        break;
                    }
                    default:
                    {
                        if(blocktype < 0) [[unlikely]] { fail_illegal_blocktype(); }

                        if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::multi_value)) [[unlikely]]
                        {
                            fail_wasm1p1_feature_required(
                                op_begin,
                                static_cast<validation_module_traits_t::wasm_u32>(blocktype),
                                ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::multi_value,
                                ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                        }

                        auto const all_type_count_uz{typesec.types.size()};
                        if(static_cast<::std::uint_least64_t>(blocktype) >
                               static_cast<::std::uint_least64_t>((::std::numeric_limits<validation_module_traits_t::wasm_u32>::max)()) ||
                           static_cast<::std::size_t>(blocktype) >= all_type_count_uz) [[unlikely]]
                        {
                            // [caller-saved opcode/prefix] immediate bytes ... | code_end
                            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                            err.err_curr = op_begin;
                            err.err_selectable.illegal_type_index.type_index =
                                blocktype > 0 ? static_cast<validation_module_traits_t::wasm_u32>(blocktype) : 0u;
                            err.err_selectable.illegal_type_index.all_type_count = checked_cast_size_to_wasm_u32(all_type_count_uz);
                            err.err_code = code_validation_error_code::illegal_type_index;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                        ::uwvm2::validation::standard::wasm3::require_core3_function_type_index_policy(
                            ::std::addressof(typesec.core3_context), static_cast<::std::size_t>(blocktype),
                            op_begin, u8"blocktype", err);
                        break;
                    }
                }

                // [control opcode][complete grammar/feature/kind-checked blocktype] next ... code_end
                // [safe                                                         ] unsafe (could be code_end)
                //                                                                 ^^ code_curr: resolver reads no bytes or cursor.
                if(!resolve_decoded_block_signature(blocktype, curr_module, block_signature)) [[unlikely]]
                {
                    // The validation and runtime module views were constructed from the same finalized module. A valid
                    // validation type index that cannot resolve in runtime storage is host-state corruption.
                    runtime_storage_bug();
                }
            }};

        // One first-read i32 numeric transition shared by every Core 3 facade.
        // Dispatch bounded/read the sole opcode byte; this adapter never rereads it.
        auto const validate_i32_numeric{
            [&]<unsigned Opcode>(::uwvm2::utils::container::u8string_view op_name)
                constexpr UWVM_THROWS -> ::uwvm2::validation::standard::wasm3::validated_i32_numeric_event
            {
                namespace v = ::uwvm2::validation::standard::wasm3;
                // [code_begin ... checked opcode ...] | code_end
                // [safe same expression allocation ] | unsafe (one-past)
                //                 ^^ op_begin copies the dispatcher-bounded code_curr.
                auto const op_begin{code_curr};
                auto const source_offset{static_cast<::std::size_t>(op_begin - code_begin)};
                // The dispatch proof code_curr<code_end permits this ONE byte advance.
                ++code_curr;
                // [decoded checked opcode] remaining ... | code_end
                // [safe                  ] unsafe (could be code_end)
                //                          ^^ code_curr; no immediate exists/read follows.
                decltype(try_pop_concrete_operand()) actual{};
                auto const consume{[&]() constexpr noexcept -> v::core3_operand
                {
                    // The common kernel proves one concrete value above the current frame
                    // BEFORE this owned copy/pop; a polymorphic synthetic pop never calls it.
                    actual = try_pop_concrete_operand();
                    return {operand_core_type(actual), actual.is_unknown};
                }};
                auto const push{[&](auto) constexpr UWVM_THROWS { operand_stack_push(curr_operand_stack_value_type::i32); }};
                v::validated_i32_numeric_event event{};
                auto const result{v::transition_i32_numeric_event<Opcode>(event, is_polymorphic,
                    concrete_operand_count, consume, push, source_offset, control_flow_stack.size())};
                if(result.error == v::typed_stack_error::stack_underflow) [[unlikely]]
                { report_operand_stack_underflow(op_begin, op_name, Opcode <= 0x69u ? 1uz : 2uz); }
                if(result.error != v::typed_stack_error::ok) [[unlikely]]
                {
                    // [checked opcode] remaining ... | code_end
                    // [safe] ^^ err_curr copies op_begin; no pointer advance/dereference.
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = static_cast<wasm_value_type>(curr_operand_stack_value_type::i32);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = static_cast<wasm_value_type>(actual.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return event;
            }};

        auto const validate_i64_numeric{
            [&]<unsigned Opcode>(::uwvm2::utils::container::u8string_view op_name)
                constexpr UWVM_THROWS -> ::uwvm2::validation::standard::wasm3::validated_i64_numeric_event
            {
                namespace v = ::uwvm2::validation::standard::wasm3;
                // [code_begin ... checked opcode ...] | code_end
                // [safe same expression allocation ] | unsafe (one-past)
                //                 ^^ op_begin copies the dispatcher-bounded code_curr.
                auto const op_begin{code_curr};
                auto const source_offset{static_cast<::std::size_t>(op_begin - code_begin)};
                // The dispatch proof code_curr<code_end permits this ONE byte advance.
                ++code_curr;
                // [decoded checked opcode] remaining ... | code_end
                // [safe                  ] unsafe (could be code_end)
                //                          ^^ code_curr; no immediate exists/read follows.
                decltype(try_pop_concrete_operand()) actual{};
                auto const consume{[&]() constexpr noexcept -> v::core3_operand
                {
                    // The common kernel proves one concrete value above the current frame
                    // BEFORE this owned copy/pop; a polymorphic synthetic pop never calls it.
                    actual = try_pop_concrete_operand();
                    return {operand_core_type(actual), actual.is_unknown};
                }};
                auto const push{[&](auto) constexpr UWVM_THROWS { operand_stack_push(curr_operand_stack_value_type::i64); }};
                v::validated_i64_numeric_event event{};
                auto const result{v::transition_i64_numeric_event<Opcode>(event, is_polymorphic,
                    concrete_operand_count, consume, push, source_offset, control_flow_stack.size())};
                if(result.error == v::typed_stack_error::stack_underflow) [[unlikely]]
                { report_operand_stack_underflow(op_begin, op_name, Opcode <= 0x7bu ? 1uz : 2uz); }
                if(result.error != v::typed_stack_error::ok) [[unlikely]]
                {
                    // [checked opcode] remaining ... | code_end
                    // [safe] ^^ err_curr copies op_begin; no pointer advance/dereference.
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = static_cast<wasm_value_type>(curr_operand_stack_value_type::i64);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = static_cast<wasm_value_type>(actual.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return event;
            }};

        auto const validate_integer_width{
            [&]<unsigned Opcode>(::uwvm2::utils::container::u8string_view op_name)
                constexpr UWVM_THROWS -> ::uwvm2::validation::standard::wasm3::validated_integer_width_event
            {
                namespace v = ::uwvm2::validation::standard::wasm3;
                constexpr auto expected_type{Opcode == 0xa7u ? curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
                constexpr auto result_type{Opcode == 0xa7u ? curr_operand_stack_value_type::i32 : curr_operand_stack_value_type::i64};
                // [code_begin ... checked opcode ...] | code_end
                // [safe same expression allocation ] | unsafe (one-past)
                //                 ^^ op_begin copies the dispatcher-bounded code_curr.
                auto const op_begin{code_curr};
                auto const source_offset{static_cast<::std::size_t>(op_begin - code_begin)};
                // The dispatch proof code_curr<code_end permits this ONE byte advance.
                ++code_curr;
                // [decoded checked opcode] remaining ... | code_end
                // [safe                  ] unsafe (could be code_end)
                //                          ^^ code_curr; no immediate exists/read follows.
                decltype(try_pop_concrete_operand()) actual{};
                auto const consume{[&]() constexpr noexcept -> v::core3_operand
                {
                    // The common kernel proves one concrete value above the current frame
                    // BEFORE this owned copy/pop; a polymorphic synthetic pop never calls it.
                    actual = try_pop_concrete_operand();
                    return {operand_core_type(actual), actual.is_unknown};
                }};
                auto const push{[&](auto) constexpr UWVM_THROWS { operand_stack_push(result_type); }};
                v::validated_integer_width_event event{};
                auto const result{v::transition_integer_width_event<Opcode>(event, is_polymorphic,
                    concrete_operand_count, consume, push, source_offset, control_flow_stack.size())};
                if(result.error == v::typed_stack_error::stack_underflow) [[unlikely]]
                { report_operand_stack_underflow(op_begin, op_name, 1uz); }
                if(result.error != v::typed_stack_error::ok) [[unlikely]]
                {
                    // [checked opcode] remaining ... | code_end
                    // [safe] ^^ err_curr copies op_begin; no pointer advance/dereference.
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = static_cast<wasm_value_type>(expected_type);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = static_cast<wasm_value_type>(actual.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return event;
            }};

        auto const validate_integer_compare{
            [&]<unsigned Opcode>(::uwvm2::utils::container::u8string_view op_name)
                constexpr UWVM_THROWS -> ::uwvm2::validation::standard::wasm3::validated_integer_compare_event
            {
                namespace v = ::uwvm2::validation::standard::wasm3;
                constexpr auto expected_type{Opcode <= 0x4fu ? curr_operand_stack_value_type::i32 : curr_operand_stack_value_type::i64};
                constexpr auto result_type{curr_operand_stack_value_type::i32};
                // [code_begin ... checked opcode ...] | code_end
                // [safe same expression allocation ] | unsafe (one-past)
                //                 ^^ op_begin copies the dispatcher-bounded code_curr.
                auto const op_begin{code_curr};
                auto const source_offset{static_cast<::std::size_t>(op_begin - code_begin)};
                // The dispatch proof code_curr<code_end permits this ONE byte advance.
                ++code_curr;
                // [decoded checked opcode] remaining ... | code_end
                // [safe                  ] unsafe (could be code_end)
                //                          ^^ code_curr; no immediate exists/read follows.
                decltype(try_pop_concrete_operand()) actual{};
                auto const consume{[&]() constexpr noexcept -> v::core3_operand
                {
                    // The common kernel proves one concrete value above the current frame
                    // BEFORE this owned copy/pop; a polymorphic synthetic pop never calls it.
                    actual = try_pop_concrete_operand();
                    return {operand_core_type(actual), actual.is_unknown};
                }};
                auto const push{[&](auto) constexpr UWVM_THROWS { operand_stack_push(result_type); }};
                v::validated_integer_compare_event event{};
                auto const result{v::transition_integer_compare_event<Opcode>(event, is_polymorphic,
                    concrete_operand_count, consume, push, source_offset, control_flow_stack.size())};
                if(result.error == v::typed_stack_error::stack_underflow) [[unlikely]]
                { report_operand_stack_underflow(op_begin, op_name, Opcode == 0x45u || Opcode == 0x50u ? 1uz : 2uz); }
                if(result.error != v::typed_stack_error::ok) [[unlikely]]
                {
                    // [checked opcode] remaining ... | code_end
                    // [safe] ^^ err_curr copies op_begin; no pointer advance/dereference.
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = static_cast<wasm_value_type>(expected_type);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = static_cast<wasm_value_type>(actual.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return event;
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

                if(operand.from_stack && !operand.is_unknown && operand.type != expected_operand_type) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                if(rhs.from_stack && !rhs.is_unknown && rhs.type != expected_operand_type) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = static_cast<wasm_value_type>(expected_operand_type);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = static_cast<wasm_value_type>(rhs.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                auto const lhs{try_pop_concrete_operand()};
                if(lhs.from_stack && !lhs.is_unknown && lhs.type != expected_operand_type) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin;
                    err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                    err.err_selectable.numeric_operand_type_mismatch.expected_type = static_cast<wasm_value_type>(expected_operand_type);
                    err.err_selectable.numeric_operand_type_mismatch.actual_type = static_cast<wasm_value_type>(lhs.type);
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                operand_stack_push(result_type);
            }};

        auto const memory_address_type_at{[&](::std::uint_least32_t index) constexpr noexcept
        {
            // [imported declarations][local declarations] are parser-owned.
            // The bounded memarg decoder checks the combined index before lookup.
            auto const& memory{index < imported_memory_count ?
                imported_memories.index_unchecked(index)->imports.storage.memory :
                memsec.memories.index_unchecked(index - imported_memory_count)};
            return memory.address64 ? ::uwvm2::validation::standard::wasm3::storage_address_type::i64 :
                ::uwvm2::validation::standard::wasm3::storage_address_type::i32;
        }};
        // The constant shared declaration gate already admitted every memory64 declaration.
        auto const table_address_type_at{[&](::std::uint_least32_t index) constexpr noexcept
        {
            // [imported declarations][local declarations] are parser-owned.
            // The table-count loop or bounded table-immediate decoder checks the index before lookup.
            auto const& table{index < imported_table_count ?
                imported_tables.index_unchecked(index)->imports.storage.table :
                tablesec.tables.index_unchecked(index - imported_table_count)};
            return table.address64 ? ::uwvm2::validation::standard::wasm3::storage_address_type::i64 :
                ::uwvm2::validation::standard::wasm3::storage_address_type::i32;
        }};
        // The constant shared declaration gate already admitted every table64 declaration.
        auto const table_operand_type{[&](::std::uint_least32_t index) constexpr noexcept
        {
            return table_address_type_at(index) == ::uwvm2::validation::standard::wasm3::storage_address_type::i64 ?
                curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32;
        }};
        auto const validate_table_operand{[&](::std::byte const* op_begin,
            ::uwvm2::utils::container::u8string_view name, curr_operand_stack_value_type expected) constexpr UWVM_THROWS
        {
            if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, name, 1uz); }
            auto const value{try_pop_concrete_operand()};
            if(value.from_stack && !value.is_unknown && value.type != expected) [[unlikely]]
            {
                // [opcode] immediates ... code_end
                // [safe  ] unsafe; borrow the dispatch-checked byte for diagnostics.
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                err.err_selectable.numeric_operand_type_mismatch = {
                    .op_code_name = name, .expected_type = to_wasm1_diagnostic_value_type(expected),
                    .actual_type = to_wasm1_diagnostic_value_type(value.type)};
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }};
        auto const validate_checked_scalar_memory{[&](::std::byte const* op_begin,
            ::uwvm2::utils::container::u8string_view op_name,
            ::uwvm2::validation::standard::wasm3::typed_memory_argument const& memarg,
            bool store, curr_operand_stack_value_type scalar_value_type) constexpr UWVM_THROWS
        {
            namespace scalar = ::uwvm2::validation::standard::wasm3;
            decltype(try_pop_concrete_operand()) actual_scalar_operand{};
            auto const consume_scalar_operand{[&]() constexpr noexcept
            {
                // The shared entire-arity preflight and bounded concrete-pop proof
                // authorize one current-frame owned operand removal, not a guest access.
                actual_scalar_operand = try_pop_concrete_operand();
                return scalar::core3_operand{scalar::core3_operand_effective_type(actual_scalar_operand),
                    !actual_scalar_operand.from_stack || actual_scalar_operand.is_unknown};
            }};
            auto const failure{scalar::validate_scalar_memory_operand_sequence(memarg, store,
                scalar::core3_legacy_carrier_type(scalar_value_type), is_polymorphic,
                concrete_operand_count, consume_scalar_operand)};
            if(failure.error == scalar::typed_stack_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, op_name, store ? 2uz : 1uz); }
            if(failure.error != scalar::typed_stack_error::ok) [[unlikely]]
            {
                // [dispatch-checked opcode][bounded checked memarg] | code_end
                // [safe                  ][safe                  ] | one-past not read
                // ^^ op_begin -> err.err_curr: copy the original borrowed diagnostic span.
                err.err_curr = op_begin;
                if(store && failure.failed_pop_index == 0u)
                {
                    err.err_selectable.store_value_type_mismatch = {.op_code_name = op_name,
                        .expected_type = to_wasm1_diagnostic_value_type(scalar_value_type), .actual_type = to_wasm1_diagnostic_value_type(actual_scalar_operand.type)};
                    err.err_code = code_validation_error_code::store_value_type_mismatch;
                }
                else if(memarg.address_type == scalar::storage_address_type::i64)
                {
                    err.err_selectable.numeric_operand_type_mismatch = {.op_code_name = op_name,
                        .expected_type = to_wasm1_diagnostic_value_type(curr_operand_stack_value_type::i64), .actual_type = to_wasm1_diagnostic_value_type(actual_scalar_operand.type)};
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                }
                else
                {
                    err.err_selectable.memarg_address_type_not_i32 = {.op_code_name = op_name,
                        .addr_type = to_wasm1_diagnostic_value_type(actual_scalar_operand.type)};
                    err.err_code = code_validation_error_code::memarg_address_type_not_i32;
                }
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }};

        auto const validate_checked_memory_page{[&](::std::byte const* op_begin,
            ::uwvm2::validation::standard::wasm3::storage_address_type address_type, bool grow) constexpr UWVM_THROWS
        {
            namespace page = ::uwvm2::validation::standard::wasm3;
            decltype(try_pop_concrete_operand()) actual_page_operand{};
            auto const consume_page_operand{[&]() constexpr noexcept
            {
                // The common sequence's reachable arity and concrete-pop proof bound
                // this current-frame owned top. Size never invokes this callback.
                actual_page_operand = try_pop_concrete_operand();
                return page::core3_operand{page::core3_operand_effective_type(actual_page_operand),
                    !actual_page_operand.from_stack || actual_page_operand.is_unknown};
            }};
            auto const failure{page::validate_memory_page_operand_sequence(
                address_type, grow, is_polymorphic, concrete_operand_count, consume_page_operand)};
            if(failure.error == page::typed_stack_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, u8"memory.grow", 1uz); }
            if(failure.error != page::typed_stack_error::ok) [[unlikely]]
            {
                // [dispatch-checked page opcode][bounded checked memidx] | code_end
                // [safe                        ][safe                 ] | one-past not read
                // ^^ op_begin -> err.err_curr: borrow only the original diagnostic span.
                err.err_curr = op_begin;
                if(address_type == page::storage_address_type::i64)
                {
                    err.err_selectable.numeric_operand_type_mismatch = {.op_code_name = u8"memory.grow",
                        .expected_type = to_wasm1_diagnostic_value_type(curr_operand_stack_value_type::i64),
                        .actual_type = to_wasm1_diagnostic_value_type(actual_page_operand.type)};
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                }
                else
                {
                    err.err_selectable.memory_grow_delta_type_not_i32.delta_type = to_wasm1_diagnostic_value_type(actual_page_operand.type);
                    err.err_code = code_validation_error_code::memory_grow_delta_type_not_i32;
                }
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }};

        auto const validate_mem_load{[&](::uwvm2::utils::container::u8string_view op_name,
                                         ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const max_align,
                                         curr_operand_stack_value_type const result_type) constexpr UWVM_THROWS
                                     {
                                         // op_name memarg ...
                                         // [safe ] unsafe (could be code_end); dispatch checked the opcode byte.
                                         auto const op_begin{code_curr};
                                         ++code_curr;
                                         // op_name memarg ...
                                         // [safe ] unsafe (could be code_end)
                                         //         ^^ code_curr
                                         // The bounded scanner checks flags, optional memory index and offset before committing code_curr.
                                         auto const memarg{::uwvm2::validation::standard::wasm3::read_memory_argument64(
                                             code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, ::uwvm2::validation::standard::wasm3::uses_core3_validation_policy(wasm1p1_para), all_memory_count, memory_address_type_at, max_align, op_name, err)};
                                         // op_name [validated memarg] ...
                                         // [safe                   ] unsafe (could be code_end)
                                         //                           ^^ code_curr
                                         validate_checked_scalar_memory(op_begin, op_name, memarg, false, result_type);

                                         operand_stack_push(result_type);
                                         return memarg;
                                     }};

        auto const validate_mem_store{[&](::uwvm2::utils::container::u8string_view op_name,
                                          ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 const max_align,
                                          curr_operand_stack_value_type const expected_value_type) constexpr UWVM_THROWS
                                      {
                                          // op_name memarg ...
                                          // [safe ] unsafe (could be code_end); dispatch checked the opcode byte.
                                          auto const op_begin{code_curr};
                                          ++code_curr;
                                          // op_name memarg ...
                                          // [safe ] unsafe (could be code_end)
                                          //         ^^ code_curr
                                          // The bounded scanner checks flags, optional memory index and offset before committing code_curr.
                                          auto const memarg{::uwvm2::validation::standard::wasm3::read_memory_argument64(
                                              code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, ::uwvm2::validation::standard::wasm3::uses_core3_validation_policy(wasm1p1_para), all_memory_count, memory_address_type_at, max_align, op_name, err)};
                                          // op_name [validated memarg] ...
                                          // [safe                   ] unsafe (could be code_end)
                                          //                           ^^ code_curr
                                          validate_checked_scalar_memory(op_begin, op_name, memarg, true, expected_value_type);

                                          return memarg;
                                      }};

#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
        auto* const native_eh_work{local_func_storage.native_eh_leaf_work.get()};
        if(native_eh_work)
        {
            bool numeric{native_eh_leaf_observer::numeric_range(curr_func_type.parameter) &&
                native_eh_leaf_observer::numeric_range(curr_func_type.result)};
            for(auto const& local: local_virtual_registers)
            {
                auto const code{static_cast<unsigned>(local.type)};
                numeric &= (code == 0x7fu || code == 0x7eu || code == 0x7du || code == 0x7cu) &&
                    (!local.has_core_type || native_eh_leaf_observer::numeric_core(local.core_type));
            }
            if(have_rich_signature)
            {
                auto const& signature{typesec.owned_signatures.index_unchecked(curr_function_type_index)};
                for(auto const& value: signature.parameters) { numeric &= native_eh_leaf_observer::numeric_core(value); }
                for(auto const& value: signature.results) { numeric &= native_eh_leaf_observer::numeric_core(value); }
            }
            native_eh_work->begin(numeric, emit_llvm_jit_active && llvm_jit_emit_state.native_guest_exceptions);
            // [local_func_storage owns current work][same fused call lifetime]
            // [safe] Borrow for the emitter only; no executable pointer is stored.
            llvm_jit_emit_state.native_eh_leaf_work = native_eh_work;
        }
#endif
#include "single_func_checkpoint_observer_site_build.h"
        // The bounded tableidx decoder and table-index policy run BEFORE this
        // shared first typing. No source byte is reread or cursor modified here.
        auto const validate_table_access{
            [&]<unsigned Opcode>(::std::byte const* op_begin, auto table_index,
                ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                -> ::uwvm2::validation::standard::wasm3::validated_table_access_event
        {
            namespace v = ::uwvm2::validation::standard::wasm3;
            auto const table_type{get_table_value_type(table_index)};
            auto const original_core{get_table_core_type(table_index)};
            auto const element_type{original_core.has_type ? original_core.type : v::core3_legacy_carrier_type(table_type)};
            auto const address_type{table_operand_type(table_index)};
            decltype(try_pop_concrete_operand()) actual{};
            auto const consume{[&]() constexpr noexcept -> v::core3_operand
            {
                // Common full-arity/frame-base proof precedes this one real owned pop.
                actual = try_pop_concrete_operand();
                return {operand_core_type(actual), actual.is_unknown};
            }};
            auto const matches{[&](auto a, auto e) constexpr noexcept { return runtime_core3_value_type_matches(a, e, typesec.owned_signatures); }};
            auto const push{[&](auto) constexpr UWVM_THROWS
            {
                operand_stack_push(table_type, false, get_table_type_witness(table_index), original_core.type, original_core.has_type);
            }};
            v::validated_table_access_event event{};
            // [code_begin ... checked opcode ... complete u32 LEB] | code_curr <= code_end
            // [safe one parser-owned expression allocation] | one-past
            // Subtract only actual same-owner cursors; neither pointer advances or reads.
            auto const result{v::transition_table_access_event<Opcode>(event, is_polymorphic,
                concrete_operand_count, consume, matches, push, static_cast<::std::uint_least32_t>(table_index),
                address_type == curr_operand_stack_value_type::i64, element_type, static_cast<unsigned>(table_type),
                static_cast<::std::size_t>(op_begin - code_begin), static_cast<::std::size_t>(code_curr - op_begin),
                control_flow_stack.size())};
            if(result.error == v::typed_stack_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, op_name, Opcode == 0x25u ? 1uz : 2uz); }
            if(result.error != v::typed_stack_error::ok) [[unlikely]]
            {
                // [same checked opcode ... completed immediate] remaining ... | code_end
                // [safe same expression allocation] | one-past never dereferenced
                // ^^ op_begin -> err.err_curr: diagnostic copy only; no pointer advance.
                err.err_curr = op_begin;
                if constexpr(Opcode == 0x26u)
                {
                    if(result.failed_pop_index == 0u)
                    {
                        err.err_selectable.br_value_type_mismatch.op_code_name = op_name;
                        err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(table_type);
                        err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(actual.type);
                        err.err_code = code_validation_error_code::br_value_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                }
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(address_type);
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(actual.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            return event;
        }};

        // First complete typed-select valtype/count admission stays in the original
        // opcode handler. This shared transition never rereads bytes or advances cursors.
        auto const validate_typed_select{
            [&](::std::byte const* op_begin, curr_operand_stack_value_type result_type,
                ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type result_core_type) constexpr UWVM_THROWS
                -> ::uwvm2::validation::standard::wasm3::validated_typed_select_event
        {
            namespace v = ::uwvm2::validation::standard::wasm3;
            decltype(try_pop_concrete_operand()) actual{};
            auto const consume{[&]() constexpr noexcept -> v::core3_operand
            {
                // Common full three-operand/current-frame proof precedes this real pop.
                actual = try_pop_concrete_operand();
                return {operand_core_type(actual), actual.is_unknown};
            }};
            auto const matches{[&](auto a, auto e) constexpr noexcept { return runtime_core3_value_type_matches(a, e, typesec.owned_signatures); }};
            auto const push{[&](auto) constexpr UWVM_THROWS
            {
                operand_stack_push(result_type, false, exact_function_type_witness(result_core_type), result_core_type, true);
            }};
            v::validated_typed_select_event event{};
            // [code_begin ... original opcode ... complete count and valtype] | code_curr <= code_end
            // [safe one actual expression owner] one-past: subtraction only, no advance/read.
            auto const result{v::transition_typed_select_event(event, is_polymorphic,
                concrete_operand_count, consume, matches, push, result_core_type, static_cast<unsigned>(result_type),
                static_cast<::std::size_t>(op_begin - code_begin), static_cast<::std::size_t>(code_curr - op_begin),
                control_flow_stack.size())};
            if(result.error == v::typed_stack_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, u8"select", 3uz); }
            if(result.error != v::typed_stack_error::ok) [[unlikely]]
            {
                // [checked opcode ... complete immediate] remaining ... | code_end
                // [safe same actual expression] one-past never read
                // ^^ op_begin -> err.err_curr: diagnostic copy only, not a cursor advance.
                err.err_curr = op_begin;
                if(result.failed_pop_index == 0u)
                {
                    err.err_selectable.select_cond_type_not_i32.cond_type = to_wasm1_diagnostic_value_type(actual.type);
                    err.err_code = code_validation_error_code::select_cond_type_not_i32;
                }
                else
                {
                    err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_diagnostic_value_type(result_type);
                    err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_diagnostic_value_type(actual.type);
                    err.err_code = code_validation_error_code::select_type_mismatch;
                }
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            return event;
        }};

#include "single_func_validation_dispatch.h"
    }

    // Build the borrowed storage descriptor for a local defined function by local-function index.
    inline constexpr local_func_storage_t get_runtime_local_func_storage(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                         ::std::size_t local_function_idx,
                                                                         [[maybe_unused]] ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        auto const import_func_count{curr_module.imported_function_vec_storage.size()};
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        if(local_function_idx >= local_func_count) [[unlikely]]
        {
            // Full, lazy, and task-group callers derive this local index from the finalized local-function count.
            // A mismatch here is corrupted internal storage, not a guest code location that can be formatted as an offset.
            runtime_storage_bug();
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

    inline constexpr void validate_runtime_local_func_with_code_version_strategy(
        validation_module_storage_t const& validation_module,
        local_func_storage_t const& local_func_storage,
        ::uwvm2::validation::error::code_validation_error_impl& err,
        parser_feature_parameter_t const* validator_feature_parameter) UWVM_THROWS
    {
        parser_feature_parameter_t const default_validator_feature_parameter{};
        auto const& effective_validator_feature_parameter{
            validator_feature_parameter == nullptr ? default_validator_feature_parameter : *validator_feature_parameter};
        ::uwvm2::validation::standard::wasm3::validate_code_with_runtime_policy(validation_module,
                                                              local_func_storage.function_index,
                                                              local_func_storage.code_begin,
                                                              local_func_storage.code_end,
                                                              err,
                                                              effective_validator_feature_parameter);
    }

    // Validate one local function and optionally append its LLVM IR into the supplied module storage.
    inline constexpr local_func_storage_t compile_all_from_uwvm_local_func(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                           validation_module_storage_t const& validation_module,
                                                                           [[maybe_unused]] compile_option const& options,
                                                                           ::std::size_t local_function_idx,
                                                                           ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                           llvm_jit_module_storage_t* emitted_llvm_jit_ir_storage = nullptr,
                                                                           ::std::vector<::uwvm2::validation::standard::wasm3::validated_call_dependency>* lazy_call_dependencies = nullptr,
                                                                           bool* lazy_call_dependencies_overflow = nullptr) UWVM_THROWS
    {
        if(options.compiler_registry != nullptr)
        {
            if(options.compilation_mode != llvm_jit_compilation_mode::full) { runtime_storage_bug(); }
            bool member{};
            for(auto const& entry : *options.compiler_registry)
            { if(::std::addressof(entry.second) == ::std::addressof(curr_module)) { member=true; break; } }
            // [comparison-only actual module identity] end
            // [safe] prove explicit registry membership BEFORE constructing the
            // borrowed descriptor or reading any module vector/type/code field.
            if(!member) { runtime_storage_bug(); }
        }
        local_func_storage_t local_func_storage{get_runtime_local_func_storage(curr_module, local_function_idx, err)};
        local_func_storage.module_id = options.curr_wasm_id;
        // [native-owned complete registry of this compilation] end
        // [safe] borrowed only through this exact fused call and joined workers;
        // no source byte cursor advances and no registry is globally selected.
        local_func_storage.compiler_registry = options.compiler_registry;
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
        local_func_storage.native_eh_leaf_work = native_eh_leaf_observer::function_state::create(
            options.native_eh_leaf_active_attempt, local_func_storage);
#endif
        if(options.emit_debug_safe_points)
        {
            // [validated expression bytes ...] | code_end
            // [safe                          ] | unsafe (one-past)
            //  ^^ code_begin; integer bounds avoid advancing either pointer.
            auto const begin{reinterpret_cast<::std::uintptr_t>(local_func_storage.code_begin)};
            auto const end{reinterpret_cast<::std::uintptr_t>(local_func_storage.code_end)};
            if(begin == 0u || end <= begin) [[unlikely]] { runtime_storage_bug(); }
            auto const bytes{end - begin};
            local_func_storage.debug_safe_point_bits.resize(bytes / 8u + (bytes % 8u != 0u));
            for(auto& bit_byte : local_func_storage.debug_safe_point_bits) { bit_byte = 0u; }
        }
        ::uwvm2::runtime::checkpoint::function_plan checkpoint_work{};
        if(options.checkpoint_profile) { checkpoint_work.profile = options.checkpoint_profile; }
        // The instruction loop below performs Core 3 validation while emitting
        // LLVM IR. No whole-body validation pass precedes this traversal.

        // Validation writes tiered loop reentry metadata back into the returned local_func_storage, so callers can publish
        // OSR entry information together with the compiled function metadata.
        validate_runtime_local_func(validation_module,
                                    local_func_storage,
                                    err,
                                    emitted_llvm_jit_ir_storage,
                                    options.verify_llvm_jit_ir || static_cast<bool>(options.checkpoint_profile),
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
                                    ::std::addressof(local_func_storage.tiered_loop_reentries),
                                    options.emit_debug_safe_points,
                                    options.compilation_mode,
                                    options.debug_safe_point_granularity,
                                    options.debug_full_patchable_typed_target_base_address,
                                    options.debug_full_patchable_typed_target_count,
                                    options.emit_debug_safe_points ? ::std::addressof(local_func_storage.debug_safe_point_bits) : nullptr,
                                    options.emit_precise_gc_root_frames,
                                    options.pending_numeric_plan,
                                    options.debug_compiled_function_generation,
                                    options.checkpoint_profile ? ::std::addressof(checkpoint_work) : nullptr,
                                    lazy_call_dependencies,
                                    lazy_call_dependencies_overflow);
        // The physical emitter has completed and destroyed its local state.
        // Returned compilation DATA must never retain an outer registry borrow
        // beyond this exact fused call and joined source-owner lifetime.
        local_func_storage.compiler_registry = nullptr;
        if(options.checkpoint_profile && emitted_llvm_jit_ir_storage != nullptr && emitted_llvm_jit_ir_storage->llvm_module != nullptr)
        {
            local_func_storage.checkpoint_plan = ::uwvm2::runtime::checkpoint::sealed_function_plan::seal_compiler_metadata(
                ::std::move(checkpoint_work));
            if(!local_func_storage.checkpoint_plan)
            {
                emitted_llvm_jit_ir_storage->note_first_decline(
                    llvm_jit_compiler_decline_stage::checkpoint_seal, local_func_storage.function_index);
                emitted_llvm_jit_ir_storage->discard_emission_preserving_first_decline();
            }
        }
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
        if(local_func_storage.native_eh_leaf_work)
        {
            local_func_storage.native_eh_leaf_observation = local_func_storage.native_eh_leaf_work->finish();
            local_func_storage.native_eh_leaf_work.reset();
        }
#endif
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

    #include "single_func_pending_numeric_effects.h"

    inline constexpr bool finalize_runtime_llvm_jit_module_storage(llvm_jit_module_storage_t& module_storage, bool verify_llvm_jit_ir,
        ::uwvm2::runtime::exception::pending_experiment::numeric_entry::compile_plan const* pending_plan = nullptr,
        bool fold_proven_empty_calls = true) noexcept
    {
        module_storage.emitted = module_storage.llvm_context_holder != nullptr && module_storage.llvm_module != nullptr;
        if(!module_storage.emitted) { return true; }

        if(pending_plan != nullptr && !finalize_pending_numeric_effects(*module_storage.llvm_module,
            *pending_plan, fold_proven_empty_calls))
        {
            module_storage.note_first_decline(llvm_jit_compiler_decline_stage::module_finalize);
            module_storage.discard_emission_preserving_first_decline();
            return false;
        }

        if(!verify_llvm_jit_module(*module_storage.llvm_module, verify_llvm_jit_ir)) [[unlikely]]
        {
            module_storage.note_first_decline(llvm_jit_compiler_decline_stage::module_finalize);
            module_storage.discard_emission_preserving_first_decline();
            return false;
        }

        return true;
    }

    inline constexpr void validate_runtime_module_all_local_funcs(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                  ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                  parser_feature_parameter_t const* validator_feature_parameter) UWVM_THROWS
    {
        // Public validation-only entry used when the runtime already has finalized module storage and no LLVM output is
        // requested.
        auto const validation_module{build_runtime_validation_module(curr_module)};
        // The adapter above proves actual runtime metadata once. Admit its
        // declarations before the zero-local-function loop can return success.
        parser_feature_parameter_t const default_validator_feature_parameter{};
        auto const& effective_validator_feature_parameter{validator_feature_parameter == nullptr ?
            default_validator_feature_parameter : *validator_feature_parameter};
        require_runtime_module_declaration_policy(curr_module, effective_validator_feature_parameter, err);
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};

        for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
        {
            auto const local_func_storage{get_runtime_local_func_storage(curr_module, local_function_idx, err)};
            validate_runtime_local_func_with_code_version_strategy(validation_module, local_func_storage, err, validator_feature_parameter);
        }
    }
}  // namespace details
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
#include "single_func_native_eh_private_leaf_stage.h"
#endif

inline constexpr ::std::size_t default_small_module_code_size_threshold{512uz * 1024uz};
inline constexpr ::std::size_t default_target_task_groups_per_compile_thread{4uz};
inline constexpr ::std::size_t default_target_task_groups_per_adjusted_compile_thread{4uz};
inline constexpr ::std::size_t aggressive_target_task_groups_per_adjusted_compile_thread{5uz};

    // Preserve the public LLVM parser facade while sharing the real pure
    // whole-module declaration/body admission and its bounded cold formatter.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr bool
        validate_all_wasm_code_for_module(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
                                          ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& feature_parameter,
                                          ::uwvm2::utils::container::u8cstring_view file_name,
                                          ::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        return ::uwvm2::uwvm::runtime::validator::validate_all_wasm_code_for_module(
            module_storage, feature_parameter, file_name, module_name);
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
                                                            ::uwvm2::validation::error::code_validation_error_impl& err,
                                                            details::parser_feature_parameter_t const& validator_feature_parameter) UWVM_THROWS
{ details::validate_runtime_module_all_local_funcs(curr_module, err, ::std::addressof(validator_feature_parameter)); }

inline constexpr void validate_runtime_wasm_code_for_module(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                            ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
{ details::validate_runtime_module_all_local_funcs(curr_module, err, nullptr); }

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
    if(options.checkpoint_profile) { options.verify_llvm_jit_ir = true; } // cold, before every worker/finalizer
    full_function_symbol_t storage{};
    // Build declaration metadata once. Function bodies are validated and emitted
    // by their owning worker in one traversal; IR is published only after success.
    auto const validation_module{details::build_runtime_validation_module(curr_module)};
    details::parser_feature_parameter_t const default_feature_parameter{};
    auto const& effective_feature_parameter{options.validator_feature_parameter == nullptr ?
        default_feature_parameter : *options.validator_feature_parameter};
    // No body decode is needed to enforce an unused declaration or a module without local bodies.
    details::require_runtime_module_declaration_policy(curr_module, effective_feature_parameter, err);

    split_config = resolve_effective_compile_task_split_config(curr_module, split_config, extra_compile_threads);

#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    native_eh_leaf_observer::restore_attempt native_eh_restore{options.native_eh_leaf_active_attempt};
    auto const reset_native_eh_attempt{[&]() noexcept
    {
        bool const scope{options.record_native_eh_leaf_observations && options.compilation_mode == llvm_jit_compilation_mode::full &&
            options.verify_llvm_jit_ir && options.native_exception_target_machine != nullptr && options.pending_numeric_plan == nullptr &&
            !options.emit_debug_safe_points && options.debug_full_patchable_typed_target_base_address == 0u &&
            options.debug_full_patchable_typed_target_count == 0uz && !options.route_wasm_calls_through_runtime_bridge &&
            options.lazy_defined_raw_call_target_base_address == 0u && options.lazy_defined_raw_call_target_count == 0uz &&
            options.lazy_defined_typed_entry_target_base_address == 0u && options.lazy_defined_typed_entry_target_count == 0uz &&
            !options.lazy_defined_targets_are_atomic && !options.emit_tiered_loop_reentry_entries};
        // [existing joined attempt][fresh actual traversal domain]
        // [safe] Before any new worker observes the immutable option copy.
        options.native_eh_leaf_active_attempt = native_eh_leaf_observer::module_attempt::create(
            options.native_eh_leaf_source_owner, curr_module, options.curr_wasm_id, scope, options.native_eh_leaf_observation_limits);
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
        if(options.native_eh_leaf_active_attempt)
        { options.native_eh_leaf_active_attempt->stage_private_leaf = options.stage_native_eh_private_leaf; }
        storage.staged_native_eh_private_leaf.reset();
#endif
        storage.native_eh_leaf_observations = {};
        for(auto& local: storage.local_funcs)
        { local.native_eh_leaf_work.reset(); local.native_eh_leaf_observation.reset(); }
    }};
#endif
    auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
    storage.local_funcs.clear();
    storage.local_funcs.resize(local_func_count);

    auto const compile_local_functions_serially{
        [&]() constexpr UWVM_THROWS
        {
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
            reset_native_eh_attempt(); // Existing serial retry: never mix prior fragment observations.
#endif
            // Serial mode emits all functions into one module and is also the fallback path when parallel preparation,
            // pre-link optimization, linking, or verification fails.
            auto const emit_llvm_jit_active{
                details::try_prepare_runtime_llvm_jit_module_storage(curr_module, storage.llvm_jit_module, options.emit_unwind_call_stack_frames, options.native_exception_target_machine)};
            if(!emit_llvm_jit_active)
            { storage.llvm_jit_module.note_first_decline(llvm_jit_compiler_decline_stage::module_prepare); }
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
            static_cast<void>(details::finalize_runtime_llvm_jit_module_storage(storage.llvm_jit_module, options.verify_llvm_jit_ir,
                options.pending_numeric_plan, options.pending_numeric_fold_proven_empty_calls));
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
            native_eh_leaf_observer::close_module(storage, options.native_eh_leaf_active_attempt);
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
            storage.staged_native_eh_private_leaf = native_eh_private_leaf::staged_module::build_after_actual_full_fusion(storage, options);
#endif
#endif
        }};

    // The private numeric ABI admits only one COMPLETE same-generation module.
    // Unresolved internal declarations in parallel/partial fragments must never
    // receive a closed-graph purity proof or be relocated as executable cores.
    if(options.pending_numeric_plan != nullptr || details::should_run_local_functions_serially(curr_module, split_config, extra_compile_threads))
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

    if(!details::try_prepare_runtime_llvm_jit_module_storage(curr_module, storage.llvm_jit_module, options.emit_unwind_call_stack_frames, options.native_exception_target_machine))
    {
        compile_local_functions_serially();
        return storage;
    }

#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    reset_native_eh_attempt();
#endif
    ::uwvm2::utils::container::vector<llvm_jit_module_storage_t> task_llvm_jit_modules{};
    task_llvm_jit_modules.resize(task_groups.size());

    bool prepared_task_llvm_jit_modules{true};
    for(auto& task_llvm_jit_module: task_llvm_jit_modules)
    {
        if(!details::try_prepare_runtime_llvm_jit_module_storage(curr_module, task_llvm_jit_module, options.emit_unwind_call_stack_frames, options.native_exception_target_machine))
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
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    native_eh_leaf_observer::close_module(storage, options.native_eh_leaf_active_attempt);
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
    storage.staged_native_eh_private_leaf = native_eh_private_leaf::staged_module::build_after_actual_full_fusion(storage, options);
#endif
#endif
    return storage;
}

// Convenience path used when only the first local function should be compiled.
inline constexpr full_function_symbol_t compile_all_from_uwvm_single_func(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                          [[maybe_unused]] compile_option& options,
                                                                          ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
{
    if(options.checkpoint_profile) { options.verify_llvm_jit_ir = true; } // cold, before every worker/finalizer
    full_function_symbol_t storage{};
    if(options.pending_numeric_plan != nullptr)
    {
        // A partial fragment cannot prove the private same-generation island's
        // complete call graph or define all of its internal ABI declarations.
        // Still run the authoritative validator; emit no private executable.
        details::validate_runtime_module_all_local_funcs(curr_module, err, options.validator_feature_parameter);
        return storage;
    }
    // Reuse this path's existing adapter before its empty-product return.
    // It validates actual runtime metadata; nonempty functions incur no new
    // adapter/storage walk and no instruction prevalidation pass.
    auto const validation_module{details::build_runtime_validation_module(curr_module)};
    details::parser_feature_parameter_t const default_validator_feature_parameter{};
    auto const& effective_validator_feature_parameter{options.validator_feature_parameter == nullptr ?
        default_validator_feature_parameter : *options.validator_feature_parameter};
    details::require_runtime_module_declaration_policy(curr_module, effective_validator_feature_parameter, err);
    if(curr_module.local_defined_function_vec_storage.empty()) { return storage; }

    storage.local_funcs.reserve(1uz);
    auto const emit_llvm_jit_active{
        details::try_prepare_runtime_llvm_jit_module_storage(curr_module, storage.llvm_jit_module, options.emit_unwind_call_stack_frames, options.native_exception_target_machine)};
    storage.local_funcs.push_back(details::compile_all_from_uwvm_local_func(curr_module,
                                                                            validation_module,
                                                                            options,
                                                                            0uz,
                                                                            err,
                                                                            emit_llvm_jit_active ? ::std::addressof(storage.llvm_jit_module) : nullptr));
    static_cast<void>(details::finalize_runtime_llvm_jit_module_storage(storage.llvm_jit_module, options.verify_llvm_jit_ir));
    return storage;
}
