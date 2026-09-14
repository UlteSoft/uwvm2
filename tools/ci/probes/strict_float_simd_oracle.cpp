// Consume strict_float_oracle.cpp's independent MPFR corpus through the actual uwvm-int SIMD evaluator.
// i64 conversions have no corresponding SIMD opcode and are skipped; i32 SIMD conversions are checked below.
#include <uwvm2/runtime/compiler/shared/wasm1p1_simd.h>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
namespace v=uwvm2::runtime::compiler::shared::wasm1p1_simd_details;
using code=v::simd_code;
using u64=std::uint64_t;
template <typename F> u64 evaluate(unsigned op,u64 a,u64 b)
{
    using U=std::conditional_t<sizeof(F)==4,std::uint32_t,u64>;
    constexpr unsigned count=16/sizeof(F);
    v::lane_array<U,count> left{},right{};
    for(unsigned i{};i!=count;++i) { left.lane[i]=static_cast<U>(a); right.lane[i]=static_cast<U>(b); }
    auto x=v::store_uint_lanes<U,count>(left),y=v::store_uint_lanes<U,count>(right);
    v::wasm_v128 result{};
#define EVALUATE(Prefix) \
    switch(op) { \
        case 0: result=v::eval_full_binop<code::Prefix##_add>(x,y); break; \
        case 1: result=v::eval_full_binop<code::Prefix##_sub>(x,y); break; \
        case 2: result=v::eval_full_binop<code::Prefix##_mul>(x,y); break; \
        case 3: result=v::eval_full_binop<code::Prefix##_div>(x,y); break; \
        case 4: result=v::eval_full_unop<code::Prefix##_sqrt>(x); break; \
        default: std::abort(); }
    if(op==7)
    {
        v::lane_array<u64,2> input{{a,a}};
        result=v::eval_full_unop<code::f32x4_demote_f64x2_zero>(v::store_uint_lanes<u64,2>(input));
    }
    else if constexpr(sizeof(F)==4) { EVALUATE(f32x4) }
    else { EVALUATE(f64x2) }
#undef EVALUATE
    auto lanes=v::load_uint_lanes<U,count>(result);
    for(unsigned i{1};i!=count;++i)
    {
        U const want=op==7 && i>=2 ? U{} : lanes.lane[0];
        if(lanes.lane[i]!=want) { std::fprintf(stderr,"inconsistent SIMD lane %u\n",i); std::abort(); }
    }
    return lanes.lane[0];
}
int main()
{
    v::lane_array<std::uint32_t,4> integers{{0x01000001,0x01000003,0x7fffffff,0xffffffff}};
    auto input=v::store_uint_lanes<std::uint32_t,4>(integers);
    auto signed_lanes=v::load_uint_lanes<std::uint32_t,4>(v::eval_full_unop<code::f32x4_convert_i32x4_s>(input));
    auto unsigned_lanes=v::load_uint_lanes<std::uint32_t,4>(v::eval_full_unop<code::f32x4_convert_i32x4_u>(input));
    constexpr std::uint32_t signed_want[]{0x4b800000,0x4b800002,0x4f000000,0xbf800000};
    constexpr std::uint32_t unsigned_want[]{0x4b800000,0x4b800002,0x4f000000,0x4f800000};
    for(unsigned i{};i!=4;++i) if(signed_lanes.lane[i]!=signed_want[i] || unsigned_lanes.lane[i]!=unsigned_want[i]) return 2;
    unsigned width,op; u64 a,b,want; unsigned long count{},failures{};
    while(std::scanf("%u %u %" SCNx64 " %" SCNx64 " %" SCNx64,&width,&op,&a,&b,&want)==5)
    {
        if(op==5 || op==6) continue;
        u64 const got=width==32 ? evaluate<float>(op,a,b) : evaluate<double>(op,a,b);
        u64 const inf=width==32 ? 0x7f800000ull : 0x7ff0000000000000ull;
        u64 const fraction=width==32 ? 0x7fffffull : 0xfffffffffffffull;
        bool const nan=(want&inf)==inf && (want&fraction)!=0;
        if((!nan && got!=want) || (nan && ((got&inf)!=inf || (got&(fraction^(fraction>>1)))==0)))
        {
            if(failures++<10) std::fprintf(stderr,"%u op=%u a=%016" PRIx64 " b=%016" PRIx64 " want=%016" PRIx64 " got=%016" PRIx64 "\n",width,op,a,b,want,got);
        }
        ++count;
    }
    std::fprintf(stderr,"SIMD checked=%lu failures=%lu\n",count,failures);
    return count!=0 && failures==0 ? 0 : 1;
}
