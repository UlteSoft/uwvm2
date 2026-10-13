#ifndef UWVM_NATIVE_EH_PHASE1_OBSERVER_API_20261003_H
#define UWVM_NATIVE_EH_PHASE1_OBSERVER_API_20261003_H

// Source-only provider experiment. This leaf is not imported by production.
// The accompanying unapplied patch installs these bytes as
// libunwind/include/uwvm_eh_observer.h in the paired provider source.
// This ABI observes one authenticated fresh throw's first search prefix;
// it neither replaces diagnostics nor observes an entire original stack.

#include <stdint.h>

#ifdef __cplusplus
#define UWVM_EH_OBSERVER_NOEXCEPT noexcept
#define UWVM_EH_OBSERVER_SYMBOL(symbol) __asm__(#symbol)
extern "C" {
#else
#define UWVM_EH_OBSERVER_NOEXCEPT
#define UWVM_EH_OBSERVER_SYMBOL(symbol)
#endif

struct uwvm_eh_observer_frame_v1 {
  uintptr_t raw_ip;
  uintptr_t cfa;
  uintptr_t region_start;
  uintptr_t actual_native_header;
  uintptr_t actual_thrown_object;
  uint32_t index;
  uint32_t ip_before_instruction;
  uint32_t has_personality;
};

struct uwvm_eh_observer_finish_v1 {
  uintptr_t actual_native_header;
  uintptr_t actual_thrown_object;
  uint32_t reported_frames;
  uint32_t bound_exceeded;
  uint32_t metadata_failure;
  int native_phase1_reason;
};

// By-value integer records do not borrow an unwind cursor or provider frame.
// opaque points to host-owned, preallocated storage kept live from arm until
// the first search finishes or disarm completes. A callback must not allocate,
// throw, block, lock, register another observer, invoke guest/host user code,
// or retain a provider object address for later dereference.
typedef void (*uwvm_eh_observer_frame_fn_v1)(
    void *opaque, uint64_t generation,
    struct uwvm_eh_observer_frame_v1 record) UWVM_EH_OBSERVER_NOEXCEPT;
typedef void (*uwvm_eh_observer_finish_fn_v1)(
    void *opaque, uint64_t generation,
    struct uwvm_eh_observer_finish_v1 record) UWVM_EH_OBSERVER_NOEXCEPT;

// Returns a provider-minted nonzero thread-local generation; zero declines.
// The exact tinfo pointer must come from the pinned C++ runtime's genuine
// guest_exception type. Arm after potentially throwing setup has completed.
// The host must disarm on every exit. This source experiment supports one
// pending registration on a thread; busy/nested arm declines, not overwrite.
// It is not a guest import and must never be exported through a Wasm module.
uint64_t uwvm_eh_observer_arm_v1(
    void const *expected_type_info, void *opaque,
    uwvm_eh_observer_frame_fn_v1 frame,
    uwvm_eh_observer_finish_fn_v1 finish) UWVM_EH_OBSERVER_NOEXCEPT
    UWVM_EH_OBSERVER_SYMBOL(uwvm_eh_observer_arm_v1);
void uwvm_eh_observer_disarm_v1(uint64_t generation)
    UWVM_EH_OBSERVER_NOEXCEPT
    UWVM_EH_OBSERVER_SYMBOL(uwvm_eh_observer_disarm_v1);

// PROVIDER-ONLY association: called by the actual paired __cxa_throw after
// __cxa_init_primary_exception. Clients must not call this by guessing a
// private __cxa_exception layout. Consume/discard pending registration even
// for a mismatching native type, so a foreign setup failure cannot arm a later
// unrelated guest throw. No registration survives the first phase-1 finish.
void uwvm_eh_observer_bind_primary_v1(
    void *actual_thrown_object, void const *actual_type_info,
    void *actual_native_header) UWVM_EH_OBSERVER_NOEXCEPT
    UWVM_EH_OBSERVER_SYMBOL(uwvm_eh_observer_bind_primary_v1);

#ifdef __cplusplus
}
#endif

#undef UWVM_EH_OBSERVER_SYMBOL
#undef UWVM_EH_OBSERVER_NOEXCEPT

#endif
