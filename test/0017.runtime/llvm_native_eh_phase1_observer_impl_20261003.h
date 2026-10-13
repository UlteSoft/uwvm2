#ifndef UWVM_NATIVE_EH_PHASE1_OBSERVER_IMPL_20261003_H
#define UWVM_NATIVE_EH_PHASE1_OBSERVER_IMPL_20261003_H

// Source-only fragment; included exactly once inside the reviewed provider's
// ELF/DWARF UnwindLevel1.c scope by the unapplied companion patch.
// Not a production header. Needs that provider's config/libunwind/unwind types.
#if !defined(__ELF__) || defined(_AIX) || \
    !defined(_LIBUNWIND_SUPPORT_DWARF_UNWIND) || \
    (!defined(__GNUC__) && !defined(__clang__))
#error "UWVM observer candidate requires the reviewed ELF/DWARF provider"
#endif
#include "uwvm_eh_observer.h"

struct uwvm_eh_observer_session_v1 {
  uint64_t generation;
  void const *expected_type;
  void *opaque;
  uwvm_eh_observer_frame_fn_v1 frame;
  uwvm_eh_observer_finish_fn_v1 finish;
  void *native_header;
  void *thrown_object;
  uint32_t state; // 0 empty, 1 armed, 2 genuine primary header bound.
  uint32_t reported;
  uint32_t overflow;
  uint32_t metadata_failure;
};

// Arm touches provider TLS before a search can observe it. The host keeps this
// actual provider image execution-pinned; callbacks must not dlopen/dlclose.
static __thread struct uwvm_eh_observer_session_v1 uwvm_observer_pending_v1;
static __thread uint64_t uwvm_observer_generation_v1;

_LIBUNWIND_EXPORT uint64_t uwvm_eh_observer_arm_v1(
    void const *expected_type_info, void *opaque,
    uwvm_eh_observer_frame_fn_v1 frame,
    uwvm_eh_observer_finish_fn_v1 finish) {
  if (expected_type_info == NULL || opaque == NULL || frame == NULL ||
      finish == NULL || uwvm_observer_pending_v1.state != 0 ||
      uwvm_observer_generation_v1 == UINT64_MAX)
    return 0;
  ++uwvm_observer_generation_v1;
  // [pinned exact tinfo] [host-owned opaque ...] end [pinned callback entries]
  // [safe             ] checked nonnull; no pointee read or ownership transfer.
  // Copy pointers into owned TLS for this generation; no descriptor pointer is
  // retained. Host RAII keeps opaque alive and disarms on every exit route.
  struct uwvm_eh_observer_session_v1 next = {
      uwvm_observer_generation_v1, expected_type_info, opaque, frame, finish,
      NULL, NULL, 1, 0, 0, 0};
  // [owned current-thread TLS slot] empty by the check above; copy only values.
  // [safe                         ] no old context/header pointer survives.
  uwvm_observer_pending_v1 = next;
  return next.generation;
}

_LIBUNWIND_EXPORT void uwvm_eh_observer_disarm_v1(uint64_t generation) {
  if (generation != 0 && uwvm_observer_pending_v1.generation == generation) {
    struct uwvm_eh_observer_session_v1 empty = {0};
    // [owned TLS slot] retire all pointer aliases without dereferencing them.
    // [safe         ] only a matching minted scope can erase this slot.
    uwvm_observer_pending_v1 = empty;
  }
}

_LIBUNWIND_EXPORT void uwvm_eh_observer_bind_primary_v1(
    void *actual_thrown_object, void const *actual_type_info,
    void *actual_native_header) {
  if (uwvm_observer_pending_v1.state != 1)
    return;
  // [owned TLS armed descriptor] copy values before clearing; no pointee read.
  // [safe                      ] local descriptor lives until this return.
  struct uwvm_eh_observer_session_v1 bound = uwvm_observer_pending_v1;
  struct uwvm_eh_observer_session_v1 empty = {0};
  // [owned TLS slot] consume any attempted primary throw, including foreign.
  // [safe         ] pending context cannot attach to a later unrelated throw.
  uwvm_observer_pending_v1 = empty;
  if (actual_thrown_object == NULL || actual_native_header == NULL ||
      actual_type_info != bound.expected_type)
    return;
  // [real paired __cxa_throw object/header] current primary allocation is live.
  // [safe                               ] provider supplies exact addresses;
  // clients never calculate private-header offsets; equality reads no pointee.
  bound.native_header = actual_native_header;
  // [same genuine primary thrown object] identity only; never dereferenced.
  // [safe                             ] paired libcxxabi supplies its lifetime.
  bound.thrown_object = actual_thrown_object;
  bound.state = 2;
  // [owned TLS slot] actual-header descriptor, no guest publication.
  // [safe         ] take clears it before the first search callback.
  uwvm_observer_pending_v1 = bound;
}

static struct uwvm_eh_observer_session_v1
uwvm_observer_take_v1(_Unwind_Exception *actual_header) {
  struct uwvm_eh_observer_session_v1 result = {0};
  if (uwvm_observer_pending_v1.state == 2 &&
      uwvm_observer_pending_v1.native_header == (void *)actual_header) {
    // [owned TLS matched descriptor] actual RaiseException header checked.
    // [safe                       ] copy into this live RaiseException frame.
    result = uwvm_observer_pending_v1;
  }
  struct uwvm_eh_observer_session_v1 empty = {0};
  // [owned TLS slot] clear every pointer even on a mismatched native raise.
  // [safe         ] rethrow/foreign raises cannot reuse an old association.
  uwvm_observer_pending_v1 = empty;
  return result;
}

static void uwvm_observer_report_frame_v1(
    struct uwvm_eh_observer_session_v1 *session, unw_cursor_t *cursor,
    unw_proc_info_t const *frame_info) {
  // [owned RaiseException session] [provider-owned actual cursor/proc-info]
  // [safe                       ] caller supplies complete live objects; check
  // optional session before fields. Cursor and proc-info pointers never escape.
  if (session == NULL || session->state != 2 || session->frame == NULL ||
      session->metadata_failure != 0)
    return;
  if (session->reported == 64) {
    session->overflow = 1;
    return;
  }
  unw_word_t raw_ip = 0;
  unw_word_t cfa = 0;
  if (__unw_get_reg(cursor, UNW_REG_IP, &raw_ip) != UNW_ESUCCESS ||
      __unw_get_reg(cursor, UNW_REG_SP, &cfa) != UNW_ESUCCESS) {
    session->metadata_failure = 1;
    return; // Never change the actual native search's reason code.
  }
  int ip_before = __unw_is_signal_frame(cursor) > 0;
  struct uwvm_eh_observer_frame_v1 record = {
      (uintptr_t)raw_ip, (uintptr_t)cfa, (uintptr_t)frame_info->start_ip,
      (uintptr_t)session->native_header, (uintptr_t)session->thrown_object,
      session->reported, (uint32_t)ip_before, frame_info->handler != 0};
  ++session->reported; // Bound checked above; callback receives index < 64.
  // [host preallocated opaque] [pinned callback] both stay live through finish.
  // [safe                    ] record contains integers, no borrowed cursor;
  // contract forbids allocation, throws, locks, registration or guest reentry.
  session->frame(session->opaque, session->generation, record);
}

static void uwvm_observer_finish_search_v1(
    struct uwvm_eh_observer_session_v1 *session, _Unwind_Reason_Code reason) {
  // [owned RaiseException session] complete stack object before native phase 2.
  // [safe                       ] caller's pointer remains live through return.
  if (session->state == 2 && session->finish != NULL) {
    struct uwvm_eh_observer_finish_v1 record = {
        (uintptr_t)session->native_header, (uintptr_t)session->thrown_object,
        session->reported, session->overflow, session->metadata_failure,
        (int)reason};
    // [live host opaque] [pinned callback] no original frame has been restored.
    // [safe            ] report result unchanged, never return a new EH action.
    session->finish(session->opaque, session->generation, record);
  }
  struct uwvm_eh_observer_session_v1 empty = {0};
  // [owned session] retire every pointer before cleanup can destroy host scope.
  // [safe         ] no registration/header association survives for rethrow.
  *session = empty;
}

#endif
