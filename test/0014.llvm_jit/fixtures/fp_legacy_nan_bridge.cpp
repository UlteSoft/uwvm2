#include <uwvm2/runtime/compiler/shared/strict_float_bits.h>

extern "C" std::uint64_t uwvm_strict_float_bits_v1(std::uint64_t a, std::uint64_t b, unsigned operation) noexcept
{ return uwvm2::runtime::compiler::shared::strict_float_jit::bridge(a, b, operation); }
