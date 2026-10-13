#!/usr/bin/env python3
"""Generate dynamic production-evaluator checks using the rational Wasm oracle.

This is a compiler component test, not qualification of a Wasm execution mode.
Integer wrapper ABIs preserve signaling NaNs on x87 before sign-only operations.
"""
import argparse
from pathlib import Path
import random
from check_oracle import check
from ieee_oracle import IEEE
from numeric_cases import inputs


def generate(fixed_nearest_environment=False):
    source = [r'''#include <fast_io.h>
#include <bit>
#include <cstdint>
#include <cfenv>
#include <uwvm2/runtime/compiler/uwvm_int/optable/numeric.h>
namespace numeric = uwvm2::runtime::compiler::uwvm_int::optable::numeric_details;
namespace strict = uwvm2::runtime::compiler::shared::strict_float;
using f32 = uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
using f64 = uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
using u32 = ::std::uint32_t;
using u64 = ::std::uint64_t;
#if UWVM2_TEST_FEATURE_PROFILE == 1
# if !defined(__loongarch64) || defined(__loongarch_sx) || defined(__loongarch_asx)
#  error Expected LoongArch64 scalar profile
# endif
#elif UWVM2_TEST_FEATURE_PROFILE == 2
# if !defined(__loongarch_sx) || defined(__loongarch_asx)
#  error Expected LSX without LASX
# endif
#elif UWVM2_TEST_FEATURE_PROFILE == 3
# if !defined(__loongarch_asx)
#  error Expected LASX
# endif
#elif UWVM2_TEST_FEATURE_PROFILE == 4
# if !defined(__i386__) || defined(__SSE2__) || defined(__SSE__)
#  error Expected i686 x87 profile
# endif
static_assert(strict::needs_extended_rounding);
#elif UWVM2_TEST_FEATURE_PROFILE == 5
# if !defined(__i386__) || !defined(__SSE2__) || defined(__SSE4_1__) || !defined(__SSE2_MATH__)
#  error Expected i686 SSE2 arithmetic without SSE4.1
# endif
static_assert(!strict::needs_extended_rounding && strict::uses_integer_rounding);
#elif UWVM2_TEST_FEATURE_PROFILE == 6
# if !defined(__i386__) || !defined(__SSE4_1__) || !defined(__SSE2_MATH__)
#  error Expected i686 SSE4.1 arithmetic
# endif
static_assert(!strict::needs_extended_rounding && !strict::uses_integer_rounding);
#else
# error An explicit instruction feature profile is required
#endif
template<typename UInt> struct unary_case { UInt a, expected, mask; };
template<typename UInt> struct binary_case { UInt a, b, expected, mask; };
template<typename UInt, ::std::size_t Count, typename Evaluator>
bool check_unary(char const* name, unary_case<UInt> const (&cases)[Count], Evaluator evaluator)
{
    for(auto entry: cases) {
        volatile UInt a{entry.a};
        auto result{evaluator(static_cast<UInt>(a))};
        if((result & entry.mask) != entry.expected) {
            ::fast_io::println(::fast_io::mnp::os_c_str(name), " mismatch a=", entry.a, " expected=", entry.expected, " mask=", entry.mask, " result=", result);
            return false;
        }
    }
    return true;
}
template<typename UInt, ::std::size_t Count, typename Evaluator>
bool check_binary(char const* name, binary_case<UInt> const (&cases)[Count], Evaluator evaluator)
{
    for(auto entry: cases) {
        volatile UInt a{entry.a}, b{entry.b};
        auto result{evaluator(static_cast<UInt>(a), static_cast<UInt>(b))};
        if((result & entry.mask) != entry.expected) {
            ::fast_io::println(::fast_io::mnp::os_c_str(name), " mismatch a=", entry.a, " b=", entry.b, " expected=", entry.expected, " mask=", entry.mask, " result=", result);
            return false;
        }
    }
    return true;
}
''']
    checks, total = [], 0
    for width in (32, 64):
        ieee = IEEE(width)
        vals = inputs(ieee)
        rng = random.Random(0xC032 + width)
        vals += [rng.getrandbits(width) for _ in range(24)]
        uint, floating = f'u{width}', f'f{width}'
        literal = lambda value: f'0x{value:x}' + ('u' if width == 32 else 'ull')
        predicate = lambda expected: expected if isinstance(expected, tuple) else (expected, (1 << width) - 1)
        for op in ('sqrt', 'nearest', 'floor', 'ceil', 'trunc', 'abs', 'neg'):
            name = f'uwvm2_component_f{width}_{op}'
            expression = (f'numeric::eval_float_sign_bits<numeric::float_unop::{op}>(a)' if op in ('abs', 'neg') else
                          f'::std::bit_cast<{uint}>(numeric::eval_float_unop<numeric::float_unop::{op}>(::std::bit_cast<{floating}>(a)))')
            source.append(f'extern "C" [[gnu::noinline]] {uint} {name}({uint} a) {{ return {expression}; }}')
            rows = [', '.join(map(literal, (a, *predicate(ieee.unary(op, a))))) for a in vals]
            source.append(f'constexpr unary_case<{uint}> cases_f{width}_{op}[]{{' + ',\n'.join('{' + row + '}' for row in rows) + '};')
            checks.append(f'if(!check_unary("f{width}.{op}", cases_f{width}_{op}, {name})) return 1;')
            total += len(rows)
        for op in ('add', 'sub', 'mul', 'div', 'min', 'max', 'copysign'):
            name = f'uwvm2_component_f{width}_{op}'
            expression = ('numeric::eval_float_copysign_bits(a, b)' if op == 'copysign' else
                          f'::std::bit_cast<{uint}>(numeric::eval_float_binop<numeric::float_binop::{op}>(::std::bit_cast<{floating}>(a), ::std::bit_cast<{floating}>(b)))')
            source.append(f'extern "C" [[gnu::noinline]] {uint} {name}({uint} a, {uint} b) {{ return {expression}; }}')
            pairs = [(a, b) for a in vals[:19] for b in vals[:19]]
            pairs += [(a, vals[(i * 17 + 5) % len(vals)]) for i, a in enumerate(vals[19:])]
            rows = [', '.join(map(literal, (a, b, *predicate(ieee.binary(op, a, b))))) for a, b in pairs]
            source.append(f'constexpr binary_case<{uint}> cases_f{width}_{op}[]{{' + ',\n'.join('{' + row + '}' for row in rows) + '};')
            checks.append(f'if(!check_binary("f{width}.{op}", cases_f{width}_{op}, {name})) return 1;')
            total += len(rows)
    if fixed_nearest_environment:
        checks.append('''auto const original_rounding{::std::fegetround()};
if(original_rounding < 0) return 2;
for(int mode: {FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
    if(::std::fesetround(mode) != 0) return 2;
    bool const passed{check_unary("f32.nearest/fenv", cases_f32_nearest, uwvm2_component_f32_nearest) &&
                      check_unary("f64.nearest/fenv", cases_f64_nearest, uwvm2_component_f64_nearest)};
    if(::std::fesetround(original_rounding) != 0) return 2;
    if(!passed) return 1;
}''')
        total += 3 * len(vals) * 2
    source.append('int main() {\n' + '\n'.join(checks) +
                  f'\n::fast_io::println("numeric component: {total} exact/allowed-NaN results passed; feature-profile=", UWVM2_TEST_FEATURE_PROFILE, " extended-rounding=", strict::needs_extended_rounding, " integer-rounding=", strict::uses_integer_rounding);\n}}')
    return '\n'.join(source) + '\n', total


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--fixed-nearest-environment', action='store_true',
                        help='also check nearest under three nondefault rounding modes; does not qualify the VM FP guard')
    args = parser.parse_args()
    check()
    source, count = generate(args.fixed_nearest_environment)
    with args.output.open('x') as stream:
        stream.write(source)
    print('Generated dynamic numeric component checks:', count, flush=True)
