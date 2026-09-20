// Use the production integer ABI in a separate native translation unit.
// Substituting an ordinary FP-return libm call would test a different ABI and
// can hide the extra rounding/quieting that the bridge is designed to prevent.
#include <uwvm2/runtime/compiler/shared/strict_float_bits.h>

extern "C" std::uint64_t uwvm_strict_float_bits_v1(std::uint64_t a, std::uint64_t b, unsigned operation) noexcept
{ return uwvm2::runtime::compiler::shared::strict_float_jit::bridge(a, b, operation); }
