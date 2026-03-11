// Note: This is a compiler-side, standalone copy of the wasm1 validator logic.
// It validates decayed `wasm_module_storage_t` (not parser storage) and must not depend on the standard validator implementation.

using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
using wasm_byte = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte;
using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
using code_validation_error_code = ::uwvm2::validation::error::code_validation_error_code;

// no necessary to set err to default

enum class block_type : unsigned
{
    function,
    block,
    loop,
    if_,
    else_
};

struct block_result_type
{
    wasm_value_type const* begin{};
    wasm_value_type const* end{};
};

struct operand_stack_storage_t
{
    wasm_value_type type{};
};

struct block_t
{
    block_result_type result{};
    ::std::size_t operand_stack_base{};
    block_type type{};
    bool polymorphic_base{};
    bool then_polymorphic_end{};  // only meaningful for if/else frames

    // Stack-top cache snapshot at "end label" entry (used when fallthrough is unreachable at `end`,
    // but the construct is reachable via an earlier branch to its end label).
    // Only meaningful when scalar stack-top caching is enabled.
    bool stacktop_has_end_state{};
    ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t stacktop_currpos_at_end{};
    ::std::size_t stacktop_memory_count_at_end{};
    ::std::size_t stacktop_cache_count_at_end{};
    ::std::size_t stacktop_cache_i32_count_at_end{};
    ::std::size_t stacktop_cache_i64_count_at_end{};
    ::std::size_t stacktop_cache_f32_count_at_end{};
    ::std::size_t stacktop_cache_f64_count_at_end{};
    // Codegen type stack snapshot at end label entry.
    // Needed to restore type information when a polymorphic fallthrough reaches `end` and becomes reachable again.
    ::uwvm2::utils::container::vector<operand_stack_storage_t> codegen_operand_stack_at_end{};

    // Stack-top cache snapshot at `if` entry (used to restore correct else-body codegen state).
    // Only meaningful for `if` frames when scalar stack-top caching is enabled.
    ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t stacktop_currpos_at_else_entry{};
    ::std::size_t stacktop_memory_count_at_else_entry{};
    ::std::size_t stacktop_cache_count_at_else_entry{};
    ::std::size_t stacktop_cache_i32_count_at_else_entry{};
    ::std::size_t stacktop_cache_i64_count_at_else_entry{};
    ::std::size_t stacktop_cache_f32_count_at_else_entry{};
    ::std::size_t stacktop_cache_f64_count_at_else_entry{};
    ::uwvm2::utils::container::vector<operand_stack_storage_t> codegen_operand_stack_at_else_entry{};

    // Stack-top cache snapshot at then-path end (used when else-path is unreachable but then-path reaches `end`).
    // Only meaningful for `if-else` frames when scalar stack-top caching is enabled.
    bool stacktop_has_then_end_state{};
    ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t stacktop_currpos_at_then_end{};
    ::std::size_t stacktop_memory_count_at_then_end{};
    ::std::size_t stacktop_cache_count_at_then_end{};
    ::std::size_t stacktop_cache_i32_count_at_then_end{};
    ::std::size_t stacktop_cache_i64_count_at_then_end{};
    ::std::size_t stacktop_cache_f32_count_at_then_end{};
    ::std::size_t stacktop_cache_f64_count_at_then_end{};
    ::uwvm2::utils::container::vector<operand_stack_storage_t> codegen_operand_stack_at_then_end{};

    // Translation labels:
    // - For `block`/`if`/`else`/`function`: `end_label_id` is the branch target.
    // - For `loop`: `start_label_id` is the branch target, `end_label_id` is the fallthrough target.
    ::std::size_t start_label_id{SIZE_MAX};
    ::std::size_t end_label_id{SIZE_MAX};
    ::std::size_t else_label_id{SIZE_MAX};  // only meaningful for if/else frames

    // Wasm bytecode pointer at the loop start label (first opcode inside the loop body). Used by extra-heavy loop fusion.
    ::std::byte const* wasm_code_curr_at_start_label{};
};

auto const import_func_count{curr_module.imported_function_vec_storage.size()};
auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};

auto const imported_global_count{static_cast<wasm_u32>(curr_module.imported_global_vec_storage.size())};
auto const local_global_count{static_cast<wasm_u32>(curr_module.local_defined_global_vec_storage.size())};
auto const all_global_count{static_cast<wasm_u32>(imported_global_count + local_global_count)};

auto const imported_table_count{static_cast<wasm_u32>(curr_module.imported_table_vec_storage.size())};
auto const local_table_count{static_cast<wasm_u32>(curr_module.local_defined_table_vec_storage.size())};
auto const all_table_count{static_cast<wasm_u32>(imported_table_count + local_table_count)};

auto const imported_memory_count{static_cast<wasm_u32>(curr_module.imported_memory_vec_storage.size())};
auto const local_memory_count{static_cast<wasm_u32>(curr_module.local_defined_memory_vec_storage.size())};
auto const all_memory_count{static_cast<wasm_u32>(imported_memory_count + local_memory_count)};

storage.local_funcs.reserve(local_func_count);
storage.local_defined_call_info.clear();
storage.local_defined_call_info.resize(local_func_count);
for(::std::size_t i{}; i != local_func_count; ++i)
{
    auto& info{storage.local_defined_call_info.index_unchecked(i)};
    info.module_id = options.curr_wasm_id;
    info.function_index = import_func_count + i;
}

// Reuse translation temporaries across functions to avoid repeated heap allocations.
using curr_block_type = block_t;
::uwvm2::utils::container::vector<curr_block_type> control_flow_stack{};

using curr_operand_stack_value_type = wasm_value_type;
using curr_operand_stack_type = ::uwvm2::utils::container::vector<operand_stack_storage_t>;
curr_operand_stack_type operand_stack{};
// Codegen operand stack (type-only): tracks the operand-stack types for the **emitted bytecode**.
// This is required for stack-top caching spill/fill typing because conbine may validate ahead of codegen.
curr_operand_stack_type codegen_operand_stack{};

::uwvm2::utils::container::vector<::std::size_t> local_offsets{};
::uwvm2::utils::container::vector<curr_operand_stack_value_type> local_types{};

// Reuse label/thunk fixup temporaries across functions to avoid repeated heap allocations.
using bytecode_vec_t = ::uwvm2::utils::container::vector<::std::byte>;

using rel_offset_t = ::std::make_unsigned_t<::std::ptrdiff_t>;
static_assert(sizeof(rel_offset_t) == sizeof(::std::byte const*));
static_assert(::std::is_trivially_copyable_v<rel_offset_t>);

struct label_info_t
{
    ::std::size_t offset{SIZE_MAX};
    bool in_thunk{};
};

struct ptr_fixup_t
{
    ::std::size_t site{};      // byte index within the owning buffer
    ::std::size_t label_id{};  // index into labels
    bool in_thunk{};           // false: site in `bytecode`, true: site in `thunks`
};

::uwvm2::utils::container::vector<label_info_t> labels{};
::uwvm2::utils::container::vector<ptr_fixup_t> ptr_fixups{};
bytecode_vec_t thunks{};
labels.reserve(64uz);
ptr_fixups.reserve(256uz);
thunks.reserve(256uz);

for(::std::size_t local_function_idx{}; local_function_idx < local_func_count; ++local_function_idx)
{
    ::std::size_t const function_index{import_func_count + local_function_idx};

    auto const& curr_local_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_idx)};
    auto const& curr_func_type{*curr_local_func.function_type_ptr};
    auto const& curr_code{*curr_local_func.wasm_code_ptr};

    auto const code_begin{reinterpret_cast<::std::byte const*>(curr_code.body.expr_begin)};
    auto const code_end{reinterpret_cast<::std::byte const*>(curr_code.body.code_end)};

    // check
    if(function_index < import_func_count) [[unlikely]]
    {
        err.err_curr = code_begin;
        err.err_selectable.not_local_function.function_index = function_index;
        err.err_code = code_validation_error_code::not_local_function;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const local_func_idx{function_index - import_func_count};
    if(local_func_idx >= local_func_count) [[unlikely]]
    {
        err.err_curr = code_begin;
        err.err_selectable.invalid_function_index.function_index = function_index;
        err.err_selectable.invalid_function_index.all_function_size = import_func_count + local_func_count;
        err.err_code = code_validation_error_code::invalid_function_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const func_parameter_begin{curr_func_type.parameter.begin};
    auto const func_parameter_end{curr_func_type.parameter.end};
    auto const func_parameter_count_uz{static_cast<::std::size_t>(func_parameter_end - func_parameter_begin)};
    auto const func_parameter_count_u32{static_cast<wasm_u32>(func_parameter_count_uz)};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
    if(func_parameter_count_u32 != func_parameter_count_uz) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

    auto const& curr_code_locals{curr_code.locals};

    // all local count = parameter + local defined local count
    wasm_u32 all_local_count{func_parameter_count_u32};
    for(auto const& local_part: curr_code_locals)
    {
        // all_local_count never overflow and never exceed the max of u32 (validated by parser limits)
        all_local_count += local_part.count;
    }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
    if constexpr(::std::numeric_limits<wasm_u32>::max() > ::std::numeric_limits<::std::size_t>::max())
    {
        if(all_local_count > ::std::numeric_limits<::std::size_t>::max()) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
    }
#endif

    // Reset per-function translation temporaries.
    control_flow_stack.clear();
    operand_stack.clear();
    codegen_operand_stack.clear();
    bool is_polymorphic{};

    ::uwvm2::runtime::compiler::uwvm_int::optable::local_func_storage_t local_func_symbol{};

    // Translation uses one internal temporary local slot to preserve single-value block results across stack-unwind branches
    // (e.g., `br`/`br_if`/`br_table` when `target_base + arity` is not equal to current stack size).
    constexpr wasm_u32 internal_temp_local_count{1u};
    wasm_u32 all_local_count_with_internal{all_local_count + internal_temp_local_count};
    local_func_symbol.local_count = static_cast<::std::size_t>(all_local_count_with_internal);

    using local_offset_t = ::std::size_t;
    constexpr local_offset_t local_slot_size{sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::wasm_stack_top_i32_i64_f32_f64_u)};
    static_assert(local_slot_size != 0uz);
    constexpr local_offset_t internal_temp_local_size{8uz};
    static_assert(local_slot_size == internal_temp_local_size);

    // Operand stack is byte-packed in byref mode: i32/f32 are 4 bytes, i64/f64 are 8 bytes.
    // We track the exact max byte usage during validation so the runtime can allocate precisely.
    auto const operand_stack_valtype_size{[&](wasm_value_type t) constexpr noexcept -> ::std::size_t
                                          {
                                              switch(t)
                                              {
                                                  case wasm_value_type::i32:
                                                  {
                                                      return 4uz;
                                                  }
                                                  case wasm_value_type::i64:
                                                  {
                                                      return 8uz;
                                                  }
                                                  case wasm_value_type::f32:
                                                  {
                                                      return 4uz;
                                                  }
                                                  case wasm_value_type::f64:
                                                  {
                                                      return 8uz;
                                                  }
                                                  [[unlikely]] default:
                                                  {
                                                      return 0uz;
                                                  }
                                              }
                                          }};

    // Current operand stack size in bytes (byte-packed: i32/f32=4, i64/f64=8).
    // Maintained incrementally to avoid O(n^2) rescans during compilation.
    ::std::size_t operand_stack_bytes{};

    // Runtime operand stack maximum (for allocation sizing).
    // Track on-demand (push/restore) instead of per-op to reduce translator overhead.
    ::std::size_t runtime_operand_stack_max{};
    ::std::size_t runtime_operand_stack_byte_max{};

    auto const operand_stack_push{[&](curr_operand_stack_value_type vt) constexpr UWVM_THROWS
                                  {
                                      // Hot path: avoid the checked `push_back()` in fast_io::vector.
                                      if(operand_stack.size() == operand_stack.capacity())
                                      {
                                          operand_stack.reserve(operand_stack.capacity() ? operand_stack.capacity() * 2uz : 64uz);
                                      }
                                      operand_stack.push_back_unchecked({vt});
                                      auto const add{operand_stack_valtype_size(vt)};
                                      if(add == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                                      if(add > (::std::numeric_limits<::std::size_t>::max() - operand_stack_bytes)) [[unlikely]]
                                      {
                                          ::fast_io::fast_terminate();
                                      }
                                      operand_stack_bytes += add;

                                      if(!is_polymorphic) [[likely]]
                                      {
                                          auto const sz{operand_stack.size()};
                                          if(sz > runtime_operand_stack_max) { runtime_operand_stack_max = sz; }
                                          if(operand_stack_bytes > runtime_operand_stack_byte_max) { runtime_operand_stack_byte_max = operand_stack_bytes; }
                                      }
                                  }};

    auto const operand_stack_pop_unchecked{[&]() constexpr noexcept
                                           {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                               if(operand_stack.empty()) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                                               if(operand_stack.empty()) { return; }
                                               auto const vt{operand_stack.back_unchecked().type};
                                               operand_stack.pop_back_unchecked();
                                               auto const sub{operand_stack_valtype_size(vt)};
                                               if(sub == 0uz || sub > operand_stack_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
                                               operand_stack_bytes -= sub;
                                           }};

    auto const operand_stack_pop_n{[&](::std::size_t n) constexpr noexcept
                                   {
                                       while(n-- != 0uz && !operand_stack.empty()) { operand_stack_pop_unchecked(); }
                                   }};

    auto const operand_stack_truncate_to{[&](::std::size_t new_size) constexpr noexcept
                                         {
                                             while(operand_stack.size() > new_size) { operand_stack_pop_unchecked(); }
                                         }};

    auto const sync_type_stacks_from_codegen_snapshot{[&](curr_operand_stack_type const& snapshot) constexpr UWVM_THROWS
                                                      {
                                                          operand_stack = snapshot;
                                                          codegen_operand_stack = snapshot;
                                                          operand_stack_bytes = 0uz;
                                                          for(auto const& v: operand_stack)
                                                          {
                                                              auto const add{operand_stack_valtype_size(v.type)};
                                                              if(add == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                                                              if(add > (::std::numeric_limits<::std::size_t>::max() - operand_stack_bytes)) [[unlikely]]
                                                              {
                                                                  ::fast_io::fast_terminate();
                                                              }
                                                              operand_stack_bytes += add;
                                                          }

                                                          if(!is_polymorphic) [[likely]]
                                                          {
                                                              auto const sz{operand_stack.size()};
                                                              if(sz > runtime_operand_stack_max) { runtime_operand_stack_max = sz; }
                                                              if(operand_stack_bytes > runtime_operand_stack_byte_max)
                                                              {
                                                                  runtime_operand_stack_byte_max = operand_stack_bytes;
                                                              }
                                                          }
                                                      }};

    // Local storage is byte-packed too (same scalar sizes). We emit local offsets as immediates, so runtime only
    // needs the total local byte size to allocate and zero-initialize.
    local_offsets.clear();
    local_types.clear();
    auto const local_offsets_need{static_cast<::std::size_t>(all_local_count_with_internal)};
    if(local_offsets.capacity() < local_offsets_need) { local_offsets.reserve(local_offsets_need); }
    // local_types stores only Wasm-visible locals (no internal temp local).
    auto const local_types_need{static_cast<::std::size_t>(all_local_count)};
    if(local_types.capacity() < local_types_need) { local_types.reserve(local_types_need); }

    auto const local_bytes_add{[&](local_offset_t& acc, local_offset_t add) constexpr noexcept
                               {
                                   if(add > (::std::numeric_limits<local_offset_t>::max() - acc)) [[unlikely]]
                                   {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                       ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                       ::fast_io::fast_terminate();
                                   }
                                   acc += add;
                               }};

    local_offset_t local_bytes{};
    for(wasm_u32 i{}; i != func_parameter_count_u32; ++i)
    {
        // Safe: reserved `all_local_count_with_internal` above.
        local_offsets.push_back_unchecked(local_bytes);
        // Safe: reserved `all_local_count` above.
        local_types.push_back_unchecked(func_parameter_begin[i]);
        local_bytes_add(local_bytes, static_cast<local_offset_t>(operand_stack_valtype_size(local_types.back_unchecked())));
    }
    for(auto const& local_part: curr_code_locals)
    {
        for(wasm_u32 j{}; j != local_part.count; ++j)
        {
            // Safe: reserved `all_local_count_with_internal` above.
            local_offsets.push_back_unchecked(local_bytes);
            // Safe: reserved `all_local_count` above.
            local_types.push_back_unchecked(local_part.type);
            local_bytes_add(local_bytes, static_cast<local_offset_t>(operand_stack_valtype_size(local_types.back_unchecked())));
        }
    }

    if(local_offsets.size() != static_cast<::std::size_t>(all_local_count)) [[unlikely]]
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
        ::fast_io::fast_terminate();
    }
    if(local_types.size() != static_cast<::std::size_t>(all_local_count)) [[unlikely]]
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
        ::fast_io::fast_terminate();
    }

    // Internal temp local comes last and must be wide enough for any scalar result (8 bytes).
    // Safe: reserved `all_local_count_with_internal` above.
    local_offsets.push_back_unchecked(local_bytes);
    local_bytes_add(local_bytes, internal_temp_local_size);

    local_func_symbol.local_bytes_max = local_bytes;

    // Local index -> byte offset from `local_base` (type...[2u]).
    auto const local_offset_from_index{[&](wasm_u32 local_index) constexpr noexcept -> local_offset_t
                                       {
                                           auto const idx{static_cast<::std::size_t>(local_index)};
                                           if(idx >= local_offsets.size()) [[unlikely]]
                                           {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                               ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                               ::fast_io::fast_terminate();
                                           }
                                           return local_offsets.index_unchecked(idx);
                                       }};

    [[maybe_unused]] auto const local_type_from_index{[&](wasm_u32 local_index) constexpr noexcept -> curr_operand_stack_value_type
                                                      {
                                                          auto const idx{static_cast<::std::size_t>(local_index)};
                                                          if(idx >= local_types.size()) [[unlikely]]
                                                          {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                              ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                                              ::fast_io::fast_terminate();
                                                          }

                                                          return local_types.index_unchecked(idx);
                                                      }};

    // Internal temp local is the first slot after all Wasm-visible locals.
    local_offset_t const internal_temp_local_off{local_offset_from_index(all_local_count)};
    // Parameters occupy the prefix of the locals buffer and are populated by memcpy at runtime.
    // Non-parameter locals must be zero-initialized by the Wasm spec, but we can skip zeroing trailing locals
    // that are never read (no `local.get`).
    local_offset_t const param_bytes_off{local_offset_from_index(func_parameter_count_u32)};
    local_offset_t local_bytes_zeroinit_end{param_bytes_off};

    static_assert(::std::is_trivially_copyable_v<::std::byte>);

    // ============================
    // Stack-top cache configuration
    // ============================

    constexpr bool stacktop_i32_enabled{CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos};
    constexpr bool stacktop_i64_enabled{CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos};
    constexpr bool stacktop_f32_enabled{CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos};
    constexpr bool stacktop_f64_enabled{CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos};
    constexpr bool stacktop_v128_enabled{CompileOption.v128_stack_top_begin_pos != CompileOption.v128_stack_top_end_pos};

    constexpr bool stacktop_enabled{stacktop_i32_enabled || stacktop_i64_enabled || stacktop_f32_enabled || stacktop_f64_enabled || stacktop_v128_enabled};

    // Wasm1 operand stack is mixed-typed. If stack-top caching is enabled here, we require full scalar coverage
    // (i32/i64/f32/f64) so the compiler-side stack-top model remains valid for arbitrary type mixes.
    if constexpr(stacktop_enabled)
    {
        static_assert(CompileOption.is_tail_call, "stack-top caching requires tail-call (non-byref) interpreter mode");

        static_assert(stacktop_i32_enabled && stacktop_i64_enabled && stacktop_f32_enabled && stacktop_f64_enabled,
                      "Wasm1 stack-top caching requires i32/i64/f32/f64 ranges enabled together.");

        static_assert(CompileOption.i32_stack_top_begin_pos >= 3uz && CompileOption.i32_stack_top_end_pos > CompileOption.i32_stack_top_begin_pos);
        static_assert(CompileOption.i64_stack_top_begin_pos >= 3uz && CompileOption.i64_stack_top_end_pos > CompileOption.i64_stack_top_begin_pos);
        static_assert(CompileOption.f32_stack_top_begin_pos >= 3uz && CompileOption.f32_stack_top_end_pos > CompileOption.f32_stack_top_begin_pos);
        static_assert(CompileOption.f64_stack_top_begin_pos >= 3uz && CompileOption.f64_stack_top_end_pos > CompileOption.f64_stack_top_begin_pos);

        // Note:
        // Smaller rings (e.g. 1 or 2 slots) are allowed. When an opcode needs more operands than the ring can hold,
        // the opfunc falls back to reading the remaining operands from the operand stack memory (stack pointer),
        // keeping as many values in registers as possible.

        // Overlaps must be *fully merged* (same begin/end). Partial overlaps are invalid for the optable layouts.
        constexpr auto overlap{[](::std::size_t a_begin, ::std::size_t a_end, ::std::size_t b_begin, ::std::size_t b_end) consteval noexcept
                               { return a_begin < b_end && b_begin < a_end; }};
        constexpr auto equal{[](::std::size_t a_begin, ::std::size_t a_end, ::std::size_t b_begin, ::std::size_t b_end) consteval noexcept
                             { return a_begin == b_begin && a_end == b_end; }};

        constexpr bool i32_i64_overlap{overlap(CompileOption.i32_stack_top_begin_pos,
                                               CompileOption.i32_stack_top_end_pos,
                                               CompileOption.i64_stack_top_begin_pos,
                                               CompileOption.i64_stack_top_end_pos)};
        constexpr bool i32_f32_overlap{overlap(CompileOption.i32_stack_top_begin_pos,
                                               CompileOption.i32_stack_top_end_pos,
                                               CompileOption.f32_stack_top_begin_pos,
                                               CompileOption.f32_stack_top_end_pos)};
        constexpr bool i32_f64_overlap{overlap(CompileOption.i32_stack_top_begin_pos,
                                               CompileOption.i32_stack_top_end_pos,
                                               CompileOption.f64_stack_top_begin_pos,
                                               CompileOption.f64_stack_top_end_pos)};
        constexpr bool i64_f32_overlap{overlap(CompileOption.i64_stack_top_begin_pos,
                                               CompileOption.i64_stack_top_end_pos,
                                               CompileOption.f32_stack_top_begin_pos,
                                               CompileOption.f32_stack_top_end_pos)};
        constexpr bool i64_f64_overlap{overlap(CompileOption.i64_stack_top_begin_pos,
                                               CompileOption.i64_stack_top_end_pos,
                                               CompileOption.f64_stack_top_begin_pos,
                                               CompileOption.f64_stack_top_end_pos)};
        constexpr bool f32_f64_overlap{overlap(CompileOption.f32_stack_top_begin_pos,
                                               CompileOption.f32_stack_top_end_pos,
                                               CompileOption.f64_stack_top_begin_pos,
                                               CompileOption.f64_stack_top_end_pos)};

        static_assert(!i32_i64_overlap || equal(CompileOption.i32_stack_top_begin_pos,
                                                CompileOption.i32_stack_top_end_pos,
                                                CompileOption.i64_stack_top_begin_pos,
                                                CompileOption.i64_stack_top_end_pos),
                      "i32/i64 stack-top ranges must be disjoint or fully merged.");
        static_assert(!i32_f32_overlap || equal(CompileOption.i32_stack_top_begin_pos,
                                                CompileOption.i32_stack_top_end_pos,
                                                CompileOption.f32_stack_top_begin_pos,
                                                CompileOption.f32_stack_top_end_pos),
                      "i32/f32 stack-top ranges must be disjoint or fully merged.");
        static_assert(!i32_f64_overlap || equal(CompileOption.i32_stack_top_begin_pos,
                                                CompileOption.i32_stack_top_end_pos,
                                                CompileOption.f64_stack_top_begin_pos,
                                                CompileOption.f64_stack_top_end_pos),
                      "i32/f64 stack-top ranges must be disjoint or fully merged.");
        static_assert(!i64_f32_overlap || equal(CompileOption.i64_stack_top_begin_pos,
                                                CompileOption.i64_stack_top_end_pos,
                                                CompileOption.f32_stack_top_begin_pos,
                                                CompileOption.f32_stack_top_end_pos),
                      "i64/f32 stack-top ranges must be disjoint or fully merged.");
        static_assert(!i64_f64_overlap || equal(CompileOption.i64_stack_top_begin_pos,
                                                CompileOption.i64_stack_top_end_pos,
                                                CompileOption.f64_stack_top_begin_pos,
                                                CompileOption.f64_stack_top_end_pos),
                      "i64/f64 stack-top ranges must be disjoint or fully merged.");
        static_assert(!f32_f64_overlap || equal(CompileOption.f32_stack_top_begin_pos,
                                                CompileOption.f32_stack_top_end_pos,
                                                CompileOption.f64_stack_top_begin_pos,
                                                CompileOption.f64_stack_top_end_pos),
                      "f32/f64 stack-top ranges must be disjoint or fully merged.");

        // v128 is used as a vector register-class carrier. If enabled, it must coincide with an f32/f64 merged range.
        if constexpr(stacktop_v128_enabled)
        {
            static_assert(CompileOption.v128_stack_top_begin_pos >= 3uz && CompileOption.v128_stack_top_end_pos > CompileOption.v128_stack_top_begin_pos);

            constexpr bool f32_v128_overlap{overlap(CompileOption.f32_stack_top_begin_pos,
                                                    CompileOption.f32_stack_top_end_pos,
                                                    CompileOption.v128_stack_top_begin_pos,
                                                    CompileOption.v128_stack_top_end_pos)};
            constexpr bool f64_v128_overlap{overlap(CompileOption.f64_stack_top_begin_pos,
                                                    CompileOption.f64_stack_top_end_pos,
                                                    CompileOption.v128_stack_top_begin_pos,
                                                    CompileOption.v128_stack_top_end_pos)};
            static_assert(f32_v128_overlap && f64_v128_overlap, "v128 range must overlap f32/f64 ranges.");
            static_assert(equal(CompileOption.v128_stack_top_begin_pos,
                                CompileOption.v128_stack_top_end_pos,
                                CompileOption.f32_stack_top_begin_pos,
                                CompileOption.f32_stack_top_end_pos) &&
                              equal(CompileOption.v128_stack_top_begin_pos,
                                    CompileOption.v128_stack_top_end_pos,
                                    CompileOption.f64_stack_top_begin_pos,
                                    CompileOption.f64_stack_top_end_pos),
                          "v128 range must fully coincide with an f32/f64 merged range (same begin/end).");

            static_assert(!overlap(CompileOption.v128_stack_top_begin_pos,
                                   CompileOption.v128_stack_top_end_pos,
                                   CompileOption.i32_stack_top_begin_pos,
                                   CompileOption.i32_stack_top_end_pos) &&
                              !overlap(CompileOption.v128_stack_top_begin_pos,
                                       CompileOption.v128_stack_top_end_pos,
                                       CompileOption.i64_stack_top_begin_pos,
                                       CompileOption.i64_stack_top_end_pos),
                          "v128 range must not overlap i32/i64 ranges.");
        }

        static_assert(details::interpreter_tuple_has_no_holes<CompileOption>(),
                      "stack-top ranges must cover all opfunc argument slots >= 3 (no holes), or shrink the ranges.");
    }

    // Translate: opfunc signature tuple (ip, operand_stack_top_ptr, local_base_ptr, [stack-top cache...]).
    // Slot types are derived from `CompileOption` to drive ABI packing (GPR vs FP/SIMD regs) correctly.
    static constexpr ::std::size_t interpreter_tuple_size{details::interpreter_tuple_size<CompileOption>()};
    using interpreter_tuple_t = decltype(details::make_interpreter_tuple<CompileOption>(::std::make_index_sequence<interpreter_tuple_size>{}));
    static constexpr interpreter_tuple_t interpreter_tuple{};

    // Translate: stack-top currpos.
    ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t curr_stacktop{
        .i32_stack_top_curr_pos = stacktop_i32_enabled ? CompileOption.i32_stack_top_begin_pos : SIZE_MAX,
        .i64_stack_top_curr_pos = stacktop_i64_enabled ? CompileOption.i64_stack_top_begin_pos : SIZE_MAX,
        .f32_stack_top_curr_pos = stacktop_f32_enabled ? CompileOption.f32_stack_top_begin_pos : SIZE_MAX,
        .f64_stack_top_curr_pos = stacktop_f64_enabled ? CompileOption.f64_stack_top_begin_pos : SIZE_MAX,
        .v128_stack_top_curr_pos = stacktop_v128_enabled ? CompileOption.v128_stack_top_begin_pos : SIZE_MAX,
    };

    // Stack-top cache runtime state (compiler-side model).
    // - `memory_count`: number of stack values materialized in operand stack memory.
    // - `cache_count`:  number of stack values resident in stack-top cache (top segment).
    ::std::size_t stacktop_memory_count{};
    ::std::size_t stacktop_cache_count{};
    ::std::size_t stacktop_cache_i32_count{};
    ::std::size_t stacktop_cache_i64_count{};
    ::std::size_t stacktop_cache_f32_count{};
    ::std::size_t stacktop_cache_f64_count{};

    // Experimental: strict control-flow entry (call-like).
    // Goal: ensure re-entry points (block/loop end, loop start, else entry) see an empty stack-top cache and
    // all values materialized in operand-stack memory, to avoid expensive state-merge/repair across edges.
    // NOTE: Keeping stack-top cache across control-flow re-entry is critical for tight loops.
    // The strict mode forces call-like barriers (flush-to-memory + reset currpos) at loop/block/else entries,
    // which can introduce large per-iteration overhead when branches are hot.
    constexpr bool strict_cf_entry_like_call{false};

    // Experimental: register-only control-flow canonicalization.
    // Goal: keep operand values resident in the stack-top cache across loop re-entry, while still making the
    // label entry currpos deterministic (range-begin) so back-edges can jump directly without spilling/filling.
    constexpr bool stacktop_regtransform_cf_entry{true};

    // Current stack-top transform opfunc supports at most:
    // - one fully-merged integer ring (i32/i64), and
    // - one fully-merged fp/simd ring (f32/f64/v128).
    constexpr bool stacktop_regtransform_supported{
        // i32/i64 must be same-range if both enabled
        (!(stacktop_i32_enabled && stacktop_i64_enabled) || (CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                                             CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos)) &&
        // f32/f64 must be same-range if both enabled
        (!(stacktop_f32_enabled && stacktop_f64_enabled) || (CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                                             CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos)) &&
        // v128 must coincide with the active f32/f64 merged range
        (!stacktop_v128_enabled || ((stacktop_f32_enabled && CompileOption.v128_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                     CompileOption.v128_stack_top_end_pos == CompileOption.f32_stack_top_end_pos) ||
                                    (stacktop_f64_enabled && CompileOption.v128_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                     CompileOption.v128_stack_top_end_pos == CompileOption.f64_stack_top_end_pos)))};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
    // Tracking for `stacktop_assert_invariants()` diagnostics.
    ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code stacktop_dbg_last_op{::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code::unreachable};
    ::std::size_t stacktop_dbg_last_ip{};
#endif

    // Bytecode emitter (writes into local_func_symbol.op.operands).
    bytecode_vec_t& bytecode{local_func_symbol.op.operands};

