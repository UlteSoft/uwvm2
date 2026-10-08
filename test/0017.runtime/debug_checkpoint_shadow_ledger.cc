/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#include <uwvm2/runtime/checkpoint/shadow_ledger.h>
#include <fast_io.h>
namespace cp = ::uwvm2::runtime::checkpoint;
static void require(bool okay, char const* description)
{
    if(!okay) { ::fast_io::io::perr("checkpoint shadow-ledger failure: ", ::fast_io::mnp::os_c_str(description), "\n"); ::fast_io::fast_terminate(); }
}
static cp::function_plan source_plan(cp::compilation_profile::owner profile)
{
    cp::function_plan plan{}; plan.profile = ::std::move(profile); plan.module = 0u; plan.function = 2u;
    plan.function_generation = 7u; plan.expression_bytes = 100u;
    cp::safepoint_layout before{}; before.identifier = 1u; before.opcode_offset = 2u; before.local_count = 2u; before.operand_count = 1u;
    before.slots = {{cp::types::core_value_type{cp::types::value_kind::i32}, true},
        {cp::types::core_value_type{cp::types::value_kind::reference, {0}, false}, false},
        {cp::types::core_value_type{cp::types::value_kind::reference, {-20}, false}, true}};
    before.controls = {{cp::control_kind::function}};
    auto awaiting{before}; awaiting.identifier = 2u; awaiting.opcode_offset = 3u;
    awaiting.phase = cp::frame_phase::awaiting_call_return; awaiting.caller_return_offset = 4u;
    auto returned{before}; returned.identifier = 3u; returned.opcode_offset = 4u;
    plan.sites = {before, awaiting, returned}; return plan;
}
int main()
{
    auto const profile{cp::compilation_profile::create_for_trusted_manager()}; require(bool(profile), "immutable opt-in profile");
    auto changed{profile->limits()}; ++changed.slots_per_frame;
    auto const other{cp::compilation_profile::create_for_trusted_manager(changed)};
    require(profile->cache_identity() != other->cache_identity(), "per-engine resource/layout policy changes cache identity");
    require(profile->cache_identity()[2u] == 6u && profile->cache_identity()[3u] == 2u, "schema6 and actual native ABI2 are cache identity inputs");
    auto const observer{cp::compilation_profile::create_for_trusted_observer()};
    require(observer && observer->limits().frames == 0u, "default observation has no physical frame quota");
    require(observer->cache_identity() != cp::compilation_profile::create_for_trusted_observer(cp::budgets{})->cache_identity(), "unlimited observation policy has distinct cache identity");
    auto zero{cp::budgets{}}; zero.frames=0u;
    require(!cp::compilation_profile::create_for_trusted_manager(zero), "resumable frames remain finite");
    auto const observation_plan{cp::sealed_function_plan::seal_compiler_metadata(source_plan(observer))};
    cp::shadow_ledger deep{observer};
    ::std::array<::std::byte, 3u * cp::native_slot_bytes> deep_slots{};
    auto const deep_ref{::uwvm2::object::global::make_wasm_i31_reference(37)};
    ::std::memcpy(deep_slots.data()+2u*cp::native_slot_bytes,::std::addressof(deep_ref),sizeof(deep_ref));
    for(::std::uint64_t i=1;i<=10000u;++i)
    { require(deep.enter({i,i-1u,i,9u},observation_plan)==cp::status::ok && deep.materialize({i,i-1u,i,9u},2u,deep_slots)==cp::status::ok,"deep default observation entry"); }
    require(deep.size()==10000u,"observation survives4096 boundary");
    auto compiler_plan{source_plan(profile)};
    require(cp::validate_plan(compiler_plan) == cp::status::ok, "exact Core3 typed before-op/await-return layouts");
    auto const plan{cp::sealed_function_plan::seal_compiler_metadata(compiler_plan)};
    require(bool(plan), "owned immutable metadata validates once before publication");
    compiler_plan.sites[0u].slots[0u].type.kind = cp::types::value_kind::f64;
    require(plan->get().sites[0u].slots[0u].type.kind == cp::types::value_kind::i32, "sealing copies caller ownership and cannot be altered through builder");
    auto malformed{source_plan(profile)}; malformed.sites[0u].slots[2u].initialized = false;
    require(cp::validate_plan(malformed) == cp::status::invalid_layout, "uninitialized slot is legal only for nondefaultable local");
    malformed = source_plan(profile); malformed.sites[0u].slots[2u].type.heap.code = cp::types::heap_type::bottom_code;
    require(cp::validate_plan(malformed) == cp::status::invalid_layout, "validation Bot is not a materializable live operand");
    ::std::array<::std::byte, 3u * cp::native_slot_bytes> slots{};
    ::std::uint32_t integer{0xabcdef12u};
    ::std::memcpy(slots.data(), ::std::addressof(integer), sizeof(integer));
    auto const i31{::uwvm2::object::global::make_wasm_i31_reference(-1)};
    static_assert(sizeof(i31) <= cp::native_slot_bytes);
    // [three complete compiler-shaped slots] end
    // [safe                                ] 2*16+sizeof(i31)<=48;
    //  ^^ the span below stays within one owned array before copying.
    ::std::memcpy(::std::span<::std::byte>{slots}.subspan(2u * cp::native_slot_bytes, cp::native_slot_bytes).data(),
        ::std::addressof(i31), sizeof(i31));
    cp::shadow_ledger ledger{profile}; cp::activation_identity root{1u, 0u, 1u, 9u};
    require(ledger.enter(root, plan) == cp::status::ok && !ledger.has_complete_materialized_frames(), "actual logical entry initially lacks materialization");
    require(ledger.materialize(root, 1u, slots) == cp::status::ok && ledger.has_complete_materialized_frames(), "typed actual slots captured including uninitialized nonnullable local");
    require(!ledger.discard_failed_recording() && ledger.size() == 1u && ledger.has_complete_materialized_frames(), "terminal data discard cannot clear healthy recording");
    require(!ledger.frames_while_actually_stopped()[0u].values[1u].declaration.initialized, "no null fabricated and no uninitialized alloca read");
    require(ledger.executable_restore_capability() == cp::status::unavailable_resume, "typed snapshot metadata alone cannot grant restore");
    ::std::size_t references{};
    require(ledger.visit_recorded_native_references([&](cp::native_reference ref)
        { ++references; return ref.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_i31; }) == cp::status::ok && references == 1u,
        "typed population keeps real i31 carrier and skips unset reference local without reading GC objects");
    require(ledger.materialize(root, 2u, slots) == cp::status::ok, "caller retained as awaiting callee return");
    cp::activation_identity child{2u, 1u, 2u, 9u};
    require(ledger.enter(child, plan) == cp::status::ok && ledger.size() == 2u, "callee parent and runtime epoch are exact");
    require(ledger.materialize(child, 1u, slots) == cp::status::ok, "callee materialized before opcode");
    require(ledger.leave(child, true) == cp::status::ok && !ledger.has_complete_materialized_frames(), "typed tail has a pending continuation until actual successor enters");
    cp::activation_identity tail{3u, 1u, 2u, 9u};
    require(ledger.enter(tail, plan) == cp::status::ok && ledger.materialize(tail, 1u, slots) == cp::status::ok, "musttail successor retains logical continuation identity");
    require(ledger.leave(tail, false) == cp::status::ok && ledger.materialize(root, 3u, slots) == cp::status::ok, "return updates caller's resumed typed stack");
    require(ledger.host_import_admission(false) == cp::status::non_replayable_import && !ledger.has_complete_materialized_frames(), "unknown host effect makes recording unavailable explicitly");
    require(ledger.visit_recorded_native_references([](cp::native_reference) { return true; }) == cp::status::non_replayable_import,
        "poisoned recording cannot pretend a complete native root census");
    require(ledger.size() == 1u && ledger.discard_failed_recording() && ledger.size() == 0u,
        "after independently qualified terminal unwind failed history drops only its owned data");
    require(ledger.failure() == cp::status::non_replayable_import && !ledger.has_complete_materialized_frames() &&
        ledger.enter(root, plan) == cp::status::non_replayable_import &&
        ledger.visit_recorded_native_references([](cp::native_reference) { return true; }) == cp::status::non_replayable_import &&
        ledger.executable_restore_capability() == cp::status::unavailable_resume,
        "discard retains sticky failure and cannot resurrect capture, GC census or restore");
    auto forged_builder{source_plan(profile)};
    forged_builder.profile = cp::compilation_profile::owner{profile.get(), [](auto*) {}};
    auto const forged_plan{cp::sealed_function_plan::seal_compiler_metadata(::std::move(forged_builder))};
    cp::shadow_ledger alias{profile};
    require(alias.enter(root, forged_plan) == cp::status::invalid_plan, "same pointer with a different controlblock cannot bind actual per-engine profile owner");
    cp::shadow_ledger wrong_epoch{profile}; require(wrong_epoch.enter(root, plan) == cp::status::ok, "fresh root");
    auto stale{root}; ++stale.runtime_epoch;
    require(wrong_epoch.materialize(stale, 1u, slots) == cp::status::invalid_activation, "numeric snapshot IDs cannot override actual activation epoch");
    cp::shadow_ledger bad_uninitialized{profile}; require(bad_uninitialized.enter(root, plan) == cp::status::ok, "fresh uninitialized-local test");
    auto poisoned{slots}; poisoned[cp::native_slot_bytes] = ::std::byte{1u};
    require(bad_uninitialized.materialize(root, 1u, poisoned) == cp::status::invalid_layout, "uninitialized native slot must contain only emitted zero padding");
    cp::shadow_ledger parser_reference{profile}; require(parser_reference.enter(root, plan) == cp::status::ok, "fresh runtime-ref test");
    ::uwvm2::object::global::wasm_global_ref_t parser{}; parser.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_func;
    ::std::memcpy(::std::span<::std::byte>{slots}.subspan(2u * cp::native_slot_bytes, cp::native_slot_bytes).data(), ::std::addressof(parser), sizeof(parser));
    require(parser_reference.materialize(root, 1u, slots) == cp::status::invalid_reference, "parser ref.func index is not a published runtime reference");
    ::fast_io::io::print("checkpoint immutable typed-plan/shadow-ledger components PASS; actual LLVM continuation/restore acceptance=false\n");
}
