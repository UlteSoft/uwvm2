// Typed continuation DATA unit. No runtime pause/returned-child/restore issuer.
#include <uwvm2/runtime/checkpoint/caller_return_projection.h>
#include <fast_io.h>
#include <array>
#include <cstring>
namespace cp = ::uwvm2::runtime::checkpoint;
static void require(bool condition, char const* text)
{
    if(!condition) { ::fast_io::io::perrln("checkpoint caller return: ", ::fast_io::mnp::os_c_str(text)); ::fast_io::fast_terminate(); }
}
static cp::types::core_value_type i31_type()
{ return {cp::types::value_kind::reference, {static_cast<::std::int_least64_t>(cp::types::abstract_heap_type::i31)}, false}; }
static cp::function_plan make_plan()
{
    cp::function_plan plan{}; plan.profile = cp::compilation_profile::create_for_trusted_manager();
    plan.function_generation = 1u; plan.expression_bytes = 16u;
    cp::safepoint_layout waiting{}; waiting.identifier = 1u; waiting.opcode_offset = 2u; waiting.caller_return_offset = 3u;
    waiting.phase = cp::frame_phase::awaiting_call_return; waiting.local_count = 2u; waiting.operand_count = 1u; waiting.saved_parameter_count = 1u;
    waiting.slots = {{{cp::types::value_kind::i64}, true}, {i31_type(), false},
        {{cp::types::value_kind::i64}, true}, {{cp::types::value_kind::i64}, true}};
    cp::control_layout function{}; function.end_offset = 15u; waiting.controls.push_back(function);
    cp::control_layout saved{}; saved.kind = cp::control_kind::if_then; saved.entry_offset = 1u; saved.end_offset = 14u;
    saved.outer_operand_height = 1u; saved.saved_parameter_count = 1u; saved.declared_parameters = {{cp::types::value_kind::i64}};
    waiting.controls.push_back(saved); auto after{waiting}; after.identifier = 2u; after.opcode_offset = 3u;
    after.phase = cp::frame_phase::before_opcode; after.caller_return_offset = 0u; after.operand_count = 4u;
    after.slots = {waiting.slots[0u], waiting.slots[1u], waiting.slots[2u],
        {{cp::types::value_kind::i64}, true}, {{cp::types::value_kind::v128}, true}, {i31_type(), true}, waiting.slots[3u]};
    plan.sites.push_back(::std::move(waiting)); plan.sites.push_back(::std::move(after)); return plan;
}
template<typename T> static cp::native_value value(cp::types::core_value_type type, T const& bits)
{
    static_assert(sizeof(T) <= cp::native_slot_bytes); cp::native_value out{}; out.declaration = {type, true};
    // [constructed owning native_value::bits[16]] end
    // [safe                                     ] sizeof(T)<=16 before copy;
    // no referenced object, token or numeric serialized address is accessed.
    ::std::memcpy(out.bits.data(), ::std::addressof(bits), sizeof(T)); return out;
}
static bool same(cp::logical_frame const& a, cp::logical_frame const& b)
{
    if(a.identity != b.identity || a.plan != b.plan || a.site != b.site || a.materialized != b.materialized || a.values.size() != b.values.size()) { return false; }
    for(::std::size_t i{}; i != a.values.size(); ++i)
    { if(a.values[i].declaration != b.values[i].declaration || a.values[i].bits != b.values[i].bits) { return false; } }
    return true;
}
int main()
{
    auto plan{make_plan()}; require(cp::validate_plan(plan) == cp::status::ok, "valid exact waiting/post-call metadata");
    auto const sealed{cp::sealed_function_plan::seal_compiler_metadata(plan)}; require(bool(sealed), "sealed immutable metadata DATA");
    ::std::array<cp::types::core_value_type, 3u> declared{{{cp::types::value_kind::i64}, {cp::types::value_kind::v128}, i31_type()}};
    auto const projection{cp::caller_return_projection::seal_compiler_data(sealed, 1u, 2u, declared)};
    require(bool(projection) && !projection->returned_child_execution_authority() &&
        projection->executable_restore_capability() == cp::status::unavailable_resume, "projection cannot mint returned-child execution authority");
    cp::logical_frame waiting{}; waiting.identity = {4u, 1u, 9u, 5u}; waiting.plan = sealed; waiting.site = 1u; waiting.materialized = true;
    waiting.values.push_back(value({cp::types::value_kind::i64}, ::std::uint64_t{30u}));
    cp::native_value unset{}; unset.declaration = {i31_type(), false}; waiting.values.push_back(unset);
    waiting.values.push_back(value({cp::types::value_kind::i64}, ::std::uint64_t{50u}));
    waiting.values.push_back(value({cp::types::value_kind::i64}, ::std::uint64_t{110u}));
    ::std::array<::std::byte, 16u> vector{};
    for(::std::size_t i{}; i != vector.size(); ++i) { vector[i] = static_cast<::std::byte>(i * 9u + 3u); }
    ::std::array<cp::native_value, 3u> returned{value(declared[0u], ::std::uint64_t{70u}), value(declared[1u], vector),
        value(declared[2u], ::uwvm2::object::global::make_wasm_i31_reference(-1))};
    cp::logical_frame output{};
    require(projection->project_typed_return_data(waiting, returned, output) == cp::status::ok && output.site == 2u &&
        output.identity == waiting.identity && output.values.size() == 7u && output.values[1u].declaration.initialized == false &&
        output.values[4u].bits == vector && output.values[6u].bits == waiting.values[3u].bits,
        "locals/prefix/results/saved params preserve actual original order and unavailable local");
    cp::native_reference ref{}; ::std::memcpy(::std::addressof(ref), output.values[5u].bits.data(), sizeof(ref));
    require(ref.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_i31 && ref.storage.wasm_i31.get_s() == -1,
        "complete nonnull i31 result carrier retained");
    auto const original{output};
    auto const reject_unchanged{[&](cp::logical_frame const& frame, ::std::span<cp::native_value const> results)
    {
        require(projection->project_typed_return_data(frame, results, output) != cp::status::ok && same(output, original),
            "rejection is failure atomic and cannot change any result frame slot");
    }};
    auto missing{waiting}; missing.values.pop_back(); reject_unchanged(missing, returned);
    auto copied_plan{waiting}; copied_plan.plan = cp::sealed_function_plan::seal_compiler_metadata(plan); reject_unchanged(copied_plan, returned);
    auto aliased_plan{waiting}; aliased_plan.plan = cp::sealed_function_plan::owner{sealed.get(), [](auto const*) noexcept {}};
    reject_unchanged(aliased_plan, returned); // Same pointer, different control block is not the retained plan.
    auto unknown_frame{waiting}; unknown_frame.identity.continuation = 0u; reject_unchanged(unknown_frame, returned);
    auto stale_site{waiting}; stale_site.site = 2u; reject_unchanged(stale_site, returned);
    auto unreadable{waiting}; unreadable.values[0u].declaration.initialized = false; reject_unchanged(unreadable, returned);
    auto forged_unset{waiting}; forged_unset.values[1u].bits[0u] = ::std::byte{1u}; reject_unchanged(forged_unset, returned);
    auto assigned{waiting}; assigned.values[1u] = value(i31_type(), ::uwvm2::object::global::make_wasm_i31_reference(17));
    cp::logical_frame assigned_output{};
    require(projection->project_typed_return_data(assigned, returned, assigned_output) == cp::status::ok &&
        assigned_output.values[1u].declaration.initialized && assigned_output.values[1u].bits == assigned.values[1u].bits,
        "actual initialized nondefaultable local survives conservative static readability proof");
    auto opaque{returned}; cp::native_reference host_ref{}; host_ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_extern;
    opaque[2u] = value(declared[2u], host_ref);
    require(projection->project_typed_return_data(waiting, opaque, output) == cp::status::unavailable_resume && same(output, original),
        "unknown host reference is never inferred to have a retained resource owner");
    reject_unchanged(waiting, ::std::span<cp::native_value const>{returned}.first(2u));
    auto wrong_result{returned}; wrong_result[2u].declaration.type.nullable = true; reject_unchanged(waiting, wrong_result);
    wrong_result = returned; wrong_result[0u].declaration.initialized = false; reject_unchanged(waiting, wrong_result);
    wrong_result = returned; cp::native_reference parser_ref{}; parser_ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_func;
    wrong_result[2u] = value(declared[2u], parser_ref); reject_unchanged(waiting, wrong_result);
    wrong_result = returned; auto noncanonical{::uwvm2::object::global::make_wasm_i31_reference(-1)};
    noncanonical.storage.wasm_i31.bits |= 0x8000'0000u; wrong_result[2u] = value(declared[2u], noncanonical); reject_unchanged(waiting, wrong_result);
    auto wrong_types{declared}; wrong_types[2u].nullable = true;
    require(!cp::caller_return_projection::seal_compiler_data(sealed, 1u, 2u, wrong_types), "semantic result type mismatch cannot use same ABI carrier");
    require(!cp::caller_return_projection::seal_compiler_data(sealed, 0u, 2u, declared) &&
        !cp::caller_return_projection::seal_compiler_data(sealed, 1u, 99u, declared), "logical site range before metadata selection");
    auto changed_control{plan}; changed_control.sites[1u].controls[1u].outer_operand_height = 0u;
    require(!cp::caller_return_projection::seal_compiler_data(cp::sealed_function_plan::seal_compiler_metadata(changed_control), 1u, 2u, declared),
        "saved control continuation must remain exact across the call");
    auto exception{plan}; exception.sites[0u].handlers.push_back({true, false, 0u, 0u, 3u});
    require(!cp::caller_return_projection::seal_compiler_data(cp::sealed_function_plan::seal_compiler_metadata(exception), 1u, 2u, declared),
        "active exception handlers require the independent real EH continuation producer");
    auto lexical{plan};
    cp::handler_layout clause{}; clause.catch_all = true; clause.target_offset = 3u;
    lexical.sites[0u].handlers.push_back(clause); lexical.sites[1u].handlers.push_back(clause);
    lexical.resume_abi_revision = 2u; lexical.resume_sites = {1u, 2u};
    auto const lexical_sealed{cp::sealed_function_plan::seal_compiler_metadata(lexical)};
    require(bool(lexical_sealed), "nested saved tuple and exact lexical handlers seal in ABI2 waiting/after pair");
    require(bool(cp::caller_return_projection::seal_compiler_data(lexical_sealed, 1u, 2u, declared)),
        "lexical EH remains equal across actual call edge; DATA still grants no native catch authority");
    auto bad_suffix{lexical}; bad_suffix.sites[1u].slots.back().type.kind = cp::types::value_kind::i32;
    require(!cp::sealed_function_plan::seal_compiler_metadata(bad_suffix),
        "same count cannot replace exact saved-if type at shifted after-call suffix");
    auto bad_clause{lexical}; bad_clause.sites[1u].handlers[0u].target_offset = 4u;
    require(!cp::sealed_function_plan::seal_compiler_metadata(bad_clause),
        "same clause count cannot change the original EH target across a waiting edge");
    auto half_catch{lexical}; half_catch.sites[0u].phase = cp::frame_phase::exception_continuation;
    require(!cp::sealed_function_plan::seal_compiler_metadata(half_catch),
        "actual half-consumed native catch still has no generated ABI2 resume entry");
    ::fast_io::io::println("checkpoint normal caller-return typed DATA component PASS; returned-child and full-VM authority=false");
}
