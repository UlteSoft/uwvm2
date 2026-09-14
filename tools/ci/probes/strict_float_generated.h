#pragma once
#include <uwvm2/runtime/compiler/shared/strict_float_bits.h>
#include <cstdint>
#include <cstdlib>
using strict_generated_call = void(*)(std::uint64_t const*,std::uint64_t const*,std::uint64_t*);
extern "C" std::uint64_t uwvm_strict_float_bits_v1(std::uint64_t a,std::uint64_t b,std::uint32_t op)
{ return uwvm2::runtime::compiler::shared::strict_float_jit::bridge(a,b,op); }
#define DECL(W,V,O) extern "C" void strict_##W##_##V##_##O(std::uint64_t const*,std::uint64_t const*,std::uint64_t*);
#define ROW(M,W,V) M(W,V,0) M(W,V,1) M(W,V,2) M(W,V,3) M(W,V,4) M(W,V,5) M(W,V,6)
ROW(DECL,32,0) DECL(32,0,7) ROW(DECL,32,1) DECL(32,1,7)
ROW(DECL,64,0) ROW(DECL,64,1)
#undef DECL
#define ADDRESS(W,V,O) &strict_##W##_##V##_##O,
inline constexpr strict_generated_call strict_generated_calls[2][2][8]{
    {{ROW(ADDRESS,32,0) &strict_32_0_7},{ROW(ADDRESS,32,1) &strict_32_1_7}},
    {{ROW(ADDRESS,64,0) nullptr},{ROW(ADDRESS,64,1) nullptr}}};
#undef ADDRESS
#undef ROW
inline std::uint64_t strict_generated(unsigned width,unsigned op,std::uint64_t a,std::uint64_t b)
{
    std::uint64_t left[4]{a,a,a,a},right[4]{b,b,b,b},out[4]{},scalar{};
    strict_generated_calls[width==64][0][op](left,right,&scalar);
    strict_generated_calls[width==64][1][op](left,right,out);
    for(auto value:out) if(value!=scalar) std::abort();
    return scalar;
}
