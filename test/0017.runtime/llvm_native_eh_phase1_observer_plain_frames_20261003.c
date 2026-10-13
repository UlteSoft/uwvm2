// Genuine C frames with CFI and no language-specific handler. The separate
// TU prevents C++ inlining/cleanup from manufacturing the handler0 witness.
// Keeper builds this with Clang C99, -fexceptions and unwind tables; it may
// propagate a genuine C++ or custom-language exception through the callback.
// This intentional propagation must NOT be declared noexcept in the C++ TU.
#if !defined(__clang__)
#error "The observer component requires Clang disable_tail_calls qualification"
#endif

__attribute__((noinline, disable_tail_calls))
void uwvm_eh_observer_plain_c_chain_v1(
    unsigned depth, void *opaque, void (*leaf)(void *)) {
  // [host-owned request ...] end [actual may-unwind leaf entry]
  // [safe                  ] the caller holds the request through catch and
  // passes a nonnull pinned entry. No pointer arithmetic or guest byte reads.
  if (depth != 0)
    uwvm_eh_observer_plain_c_chain_v1(depth - 1, opaque, leaf);
  else
    leaf(opaque);
  // The real return edge plus disable_tail_calls preserves every physical
  // activation even under O3. This is a semantic fixture, not a perf loop.
  __asm__ volatile("" : : "r"(depth), "r"(opaque) : "memory");
}
