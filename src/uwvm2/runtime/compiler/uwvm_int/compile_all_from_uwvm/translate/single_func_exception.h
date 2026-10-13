// Core 3 exception edges are cold, but must enter the exact register-ring state selected by normal
// control flow. Descriptors own their tags/layouts; only final, stable bytecode addresses are published.
// https://webassembly.github.io/spec/core/exec/instructions.html#control-instructions
#if defined(UWVM_CPP_EXCEPTIONS)
namespace exception_op = ::uwvm2::runtime::compiler::uwvm_int::optable;
namespace exception_value = ::uwvm2::runtime::exception;

struct exception_entry_state
{
    curr_operand_stack_type types{};
    decltype(curr_stacktop) positions{};
    ::std::size_t memory{}, cache{}, i32{}, i64{}, f32{}, f64{};
    bool ready{};
};
struct exception_target_record
{
    ::std::size_t label{}, thunk{};
    exception_entry_state entry{};
};
struct exception_call_fixup { ::std::size_t slot{}, call{}; };
::std::vector<exception_target_record> exception_targets{};
::std::vector<exception_op::exception_numeric_call_spec> exception_calls{};
::std::vector<exception_call_fixup> exception_call_fixups{};
::std::vector<::std::shared_ptr<exception_op::exception_throw_site const>> exception_throws{};

auto const exception_payload_kind{[&](wasm_value_type vt, ::std::byte const* opcode, unsigned opcode_value) UWVM_THROWS
    -> exception_value::payload_kind
{
    if(static_cast<unsigned>(vt) == 0x69u) { return exception_value::payload_kind::wasm_reference; }
    switch(vt)
    {
        case wasm_value_type::i32: return exception_value::payload_kind::i32;
        case wasm_value_type::i64: return exception_value::payload_kind::i64;
        case wasm_value_type::f32: return exception_value::payload_kind::f32;
        case wasm_value_type::f64: return exception_value::payload_kind::f64;
        case wasm_value_type::v128: return exception_value::payload_kind::v128;
        case wasm_value_type::funcref:
        case wasm_value_type::externref: return exception_value::payload_kind::wasm_reference;
        default: ::uwvm2::runtime::compiler::shared::wasm_exception_control::unsupported(opcode, opcode_value, err);
    }
}};

auto const exception_snapshot{[&]() UWVM_THROWS -> exception_entry_state
{
    if constexpr(stacktop_enabled)
    { return {codegen_operand_stack, curr_stacktop, stacktop_memory_count, stacktop_cache_count,
              stacktop_cache_i32_count, stacktop_cache_i64_count, stacktop_cache_f32_count, stacktop_cache_f64_count, true}; }
    else { return {operand_stack, curr_stacktop, operand_stack.size(), 0uz, 0uz, 0uz, 0uz, 0uz, true}; }
}};

auto const exception_capture_end_state{[&](block_t const& frame) UWVM_THROWS
{
    if(frame.type != block_type::loop && frame.exception_target_index != SIZE_MAX)
    { exception_targets[frame.exception_target_index].entry = exception_snapshot(); }
}};

auto const exception_has_active_handlers{[&]() noexcept
{
    if(!codegen_reachable) { return false; }
    for(auto const& frame: control_flow_stack) { if(!frame.exception_handlers.empty()) { return true; } }
    return false;
}};

auto const emit_exception_call_site{[&](::std::byte const* opcode, ::std::size_t source_bytes) UWVM_THROWS
{
    exception_op::exception_numeric_call_spec call{};
    call.stack_bytes_at_call = source_bytes;
    bool complete{};
    for(auto depth{control_flow_stack.size()}; depth != 0uz && !complete; --depth)
    {
        for(auto const& clause: control_flow_stack.index_unchecked(depth - 1uz).exception_handlers)
        {
            // target_frame was bounded against live OUTER frames by read_handlers; no frame is popped
            // while this call record is built. Numeric indices survive vector reallocations below.
            auto& target_frame{control_flow_stack.index_unchecked(clause.target_frame)};
            auto const arity{target_frame.label.begin == target_frame.label.end ? 0uz :
                static_cast<::std::size_t>(target_frame.label.end - target_frame.label.begin)};
            exception_op::exception_numeric_handler_spec handler{};
            handler.catch_all = clause.identity == nullptr;
            handler.with_reference = clause.with_reference;
            handler.receiving_store = curr_module.gc_store;
            if(!handler.catch_all) { handler.tag = clause.identity->exception_identity; }
            for(::std::size_t index{}; index != target_frame.operand_stack_base; ++index)
            {
                auto const width{operand_stack_valtype_size(operand_stack.index_unchecked(index).type)};
                if(width == 0uz || width > PTRDIFF_MAX - handler.prefix_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
                handler.prefix_bytes += width;
            }
            auto target_bytes{handler.prefix_bytes};
            auto const payload_arity{arity - static_cast<::std::size_t>(handler.with_reference)};
            for(::std::size_t index{}; index != payload_arity; ++index)
            {
                // [validated target label tuple ...] end; index < arity proves a complete type entry.
                // [safe                            ] no parser cursor is retained by runtime metadata.
                auto const kind{exception_payload_kind(target_frame.label.begin[index], opcode, 0x1fu)};
                auto const width{exception_value::payload_width(kind)};
                if(width > PTRDIFF_MAX - target_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
                target_bytes += width;
                handler.parameter_kinds.push_back(kind);
            }
            if(handler.with_reference)
            {
                constexpr auto ref_width{sizeof(::uwvm2::object::global::wasm_global_ref_t)};
                if(ref_width > PTRDIFF_MAX - target_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
                target_bytes += ref_width;
            }
            if(target_bytes > runtime_operand_stack_byte_max) { runtime_operand_stack_byte_max = target_bytes; }
            if(arity > SIZE_MAX - target_frame.operand_stack_base) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const target_count{target_frame.operand_stack_base + arity};
            if(target_count > runtime_operand_stack_max) { runtime_operand_stack_max = target_count; }

            if(target_frame.exception_target_index == SIZE_MAX)
            {
                exception_target_record target{get_branch_target_label_id(target_frame), new_label(true), {}};
                // Prefix values remain live below this lexical frame; append the validated label tuple.
                target.entry.types.reserve(target_count);
                for(::std::size_t index{}; index != target_frame.operand_stack_base; ++index)
                { target.entry.types.push_back(operand_stack.index_unchecked(index)); }
                for(::std::size_t index{}; index != arity; ++index)
                { target.entry.types.push_back({target_frame.label.begin[index]}); }
                if(target_frame.type == block_type::loop)
                {
                    // Loop headers already exist. Their small cache snapshot was saved at entry;
                    // their type prefix/parameters above are still validated in this active scope.
                    target.entry.positions = target_frame.stacktop_currpos_at_else_entry;
                    target.entry.memory = stacktop_enabled ? target_frame.stacktop_memory_count_at_else_entry : target_count;
                    target.entry.cache = target_frame.stacktop_cache_count_at_else_entry;
                    target.entry.i32 = target_frame.stacktop_cache_i32_count_at_else_entry;
                    target.entry.i64 = target_frame.stacktop_cache_i64_count_at_else_entry;
                    target.entry.f32 = target_frame.stacktop_cache_f32_count_at_else_entry;
                    target.entry.f64 = target_frame.stacktop_cache_f64_count_at_else_entry;
                    target.entry.ready = true;
                }
                target_frame.exception_target_index = exception_targets.size();
                exception_targets.push_back(::std::move(target));
            }
            // This field holds a logical thunk label until finalization, never a borrowed buffer pointer.
            handler.target_ip_offset = exception_targets[target_frame.exception_target_index].thunk;
            call.handlers.push_back(::std::move(handler));
            if(clause.identity == nullptr) { complete = true; break; }
        }
    }
    exception_call_fixups.push_back({bytecode.size(), exception_calls.size()});
    exception_calls.push_back(::std::move(call));
    emit_imm_to(bytecode, static_cast<exception_op::exception_call_site const*>(nullptr));
}};

auto const emit_exception_throw{[&](auto const& tag, ::std::byte const* opcode) UWVM_THROWS
{
    ::std::vector<exception_value::payload_kind> kinds{};
    kinds.reserve(tag.parameters.size());
    for(::std::size_t index{}; index != tag.parameters.size(); ++index)
    { kinds.push_back(exception_payload_kind(tag.parameters.begin[index], opcode, 0x08u)); }
    auto owner{exception_op::exception_throw_site::make_numeric(tag.identity->exception_identity, kinds, curr_module.gc_store)};
    if(!owner) [[unlikely]]
    { ::uwvm2::runtime::compiler::shared::wasm_exception_control::unsupported(opcode, 0x08u, err); }
    stacktop_flush_all_to_operand_stack(bytecode);
    auto const protected_throw{exception_has_active_handlers()};
    emit_opfunc_to(bytecode, protected_throw ?
        exception_op::translate::get_uwvmint_throw_catching_fptr_from_tuple<CompileOption>(interpreter_tuple) :
        exception_op::translate::get_uwvmint_throw_numeric_fptr_from_tuple<CompileOption>(interpreter_tuple));
    // [owning immutable site] its allocation does not move when exception_throws grows.
    // [safe                 ] metadata takes ownership before the function is published.
    emit_imm_to(bytecode, owner.get());
    exception_throws.push_back(::std::move(owner));
    if(protected_throw) { emit_exception_call_site(opcode, operand_stack_bytes); }
}};

auto const emit_exception_thunks{[&]() UWVM_THROWS
{
    if(exception_targets.empty()) { return; }
    auto saved{exception_snapshot()};
    auto const saved_codegen{codegen_operand_stack};
    auto const saved_polymorphic{is_polymorphic};
    for(auto const& target: exception_targets)
    {
        auto const& state{target.entry};
        if(!state.ready || state.memory > state.types.size() || state.cache != state.types.size() - state.memory) [[unlikely]]
        { ::fast_io::fast_terminate(); }
        set_label_offset(target.thunk, thunks.size());
        codegen_operand_stack = state.types;
        curr_stacktop = state.positions;
        is_polymorphic = false;
        stacktop_memory_count = state.types.size();
        stacktop_cache_count = 0uz;
        stacktop_cache_i32_count = stacktop_cache_i64_count = stacktop_cache_f32_count = stacktop_cache_f64_count = 0uz;
        if constexpr(stacktop_enabled)
        {
            // The cold dispatcher restored a fully materialized stack. Load exactly the suffix expected
            // by this label, from top to bottom, sharing range counts for merged integer/FP/SIMD rings.
            // Ordinary control-flow edges retain their existing register-only transformations.
            while(stacktop_memory_count != state.memory)
            {
                auto const vt{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};
                if(!stacktop_enabled_for_vt(vt)) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const begin{stacktop_range_begin_pos(vt)};
                auto const end{stacktop_range_end_pos(vt)};
                auto const count{stacktop_cache_count_for_range(begin, end)};
                if(count >= end - begin) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const slot{stacktop_ring_advance_next(stacktop_currpos_for_range(begin, end), count, begin, end)};
                emit_stacktop_fill1_typed_to(thunks, slot, vt);
                --stacktop_memory_count;
                ++stacktop_cache_count;
                ++stacktop_cache_count_ref_for_vt(vt);
            }
            if(stacktop_cache_i32_count != state.i32 || stacktop_cache_i64_count != state.i64 ||
               stacktop_cache_f32_count != state.f32 || stacktop_cache_f64_count != state.f64) [[unlikely]]
            { ::fast_io::fast_terminate(); }
        }
        emit_br_to(thunks, target.label, true);
    }
    codegen_operand_stack = saved_codegen;
    curr_stacktop = saved.positions;
    stacktop_memory_count = saved.memory;
    stacktop_cache_count = saved.cache;
    stacktop_cache_i32_count = saved.i32;
    stacktop_cache_i64_count = saved.i64;
    stacktop_cache_f32_count = saved.f32;
    stacktop_cache_f64_count = saved.f64;
    is_polymorphic = saved_polymorphic;
}};

auto const finalize_exception_metadata{[&](::std::size_t main_size) UWVM_THROWS
{
    if(exception_calls.empty() && exception_throws.empty()) { return; }
    for(auto& call: exception_calls)
    {
        for(auto& handler: call.handlers)
        {
            auto const& label{labels.index_unchecked(handler.target_ip_offset)};
            if(label.offset == SIZE_MAX || !label.in_thunk || label.offset > SIZE_MAX - main_size) [[unlikely]]
            { ::fast_io::fast_terminate(); }
            handler.target_ip_offset = main_size + label.offset;
        }
    }
    using opfunc_type = details::interpreter_expected_opfunc_ptr_t<CompileOption>;
    auto owner{exception_op::exception_function_metadata::make_numeric(
        {bytecode.data(), bytecode.size()}, sizeof(opfunc_type), runtime_operand_stack_byte_max, exception_calls, exception_throws)};
    if(!owner) [[unlikely]] { ::fast_io::fast_terminate(); }
    for(auto const& fixup: exception_call_fixups)
    {
        auto const site{owner->call_site(fixup.call)};
        if(site == nullptr || fixup.slot > bytecode.size() || sizeof(site) > bytecode.size() - fixup.slot) [[unlikely]]
        { ::fast_io::fast_terminate(); }
        // [final owned bytecode ...][complete pointer slot][...] end
        // [safe                    ^^^^^^^^^^^^^^^^^^^^^^] size subtraction proved the whole write.
        //                          ^^ bytecode.data() + fixup.slot; no append/reallocation follows.
        ::std::memcpy(bytecode.data() + fixup.slot, ::std::addressof(site), sizeof(site));
    }
    local_func_symbol.exception_metadata = ::std::move(owner);
}};
#else
[[maybe_unused]] auto const exception_has_active_handlers{[]() constexpr noexcept { return false; }};
#endif
