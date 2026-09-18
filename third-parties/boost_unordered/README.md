# boost_unordered (vendored / trimmed)

This directory contains a vendored, **trimmed** subset of Boost, focused on `boost_unordered`.

## Not a 1:1 copy of upstream Boost

This is not an exact mirror of upstream Boost or upstream `boost_unordered`. Files may be removed, rearranged, or patched to fit this project.

## Upstream note

This is not an official mirror of upstream `boost_unordered`. Please do not report issues found in this vendored copy to upstream unless you can reproduce them on upstream.

## Local capacity arithmetic repair (2026-09-18)

The open-addressing table's internal fixed load factor is 7/8. Capacity
calculations now use exact integer quotient/remainder arithmetic, not `float`
and `std::ceil`. This makes integer-only table operations build on x86-64 with
SSE disabled (a float-returning call still requires XMM0 in the native ABI),
including at `-O0`. It also avoids loss of precision when sizing large tables.
Public floating-point load-factor query APIs are unchanged.

The conversion, growth increment, rounded group capacity, and backing buffer
size are checked before overflow; impossible sizes throw `std::bad_alloc`, or
terminate in exception-disabled builds. They must not wrap into a small buffer.
No lookup/probing algorithm or native SIMD selection is changed. Regression:
`test/0003.utils/container/unordered_integer_capacity.cc` in the repository.
