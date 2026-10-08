#pragma once
#include <array>
#include <cstddef>
#include <cstdlib>
// Independent byte-level expectations derived from Core 3 SIMD memory semantics.
namespace memory64_simd_reference
{
struct spec{unsigned width;bool store,lane,consumes;};
constexpr spec describe(unsigned n)
{
 if(n==0||n==11)return {16,n==11,false,n==11};
 if(n<=6)return {8,false,false,false};
 if(n<=10)return {1u<<(n-7),false,false,false};
 if(n>=84&&n<=91)return {1u<<((n-84)%4),n>=88,true,true};
 if(n!=92&&n!=93){std::abort();}return {n==92?4u:8u,false,false,false};
}
inline std::array<std::byte,16> expected(unsigned op,unsigned lane,std::array<std::byte,16> const& memory,std::array<std::byte,16> old)
{
 auto spec=describe(op);std::array<std::byte,16> out{};
 if(spec.lane)
 {
  if(spec.store){for(unsigned n=0;n<spec.width;++n)out[n]=old[lane*spec.width+n];}
  else {out=old;for(unsigned n=0;n<spec.width;++n)out[lane*spec.width+n]=memory[n];}
 }
 else if(op==11){out=old;}
 else if(op==0){out=memory;}
 else if(op>=1&&op<=6)
 {
  unsigned input_width=1u<<((op-1)/2),output_width=input_width*2;
  for(unsigned lane=0;lane<8/input_width;++lane)
  {
   for(unsigned b=0;b<input_width;++b)out[lane*output_width+b]=memory[lane*input_width+b];
   auto fill=(op&1)&&(std::to_integer<unsigned>(memory[(lane+1)*input_width-1])&128)?std::byte{0xff}:std::byte{};
   for(unsigned b=input_width;b<output_width;++b)out[lane*output_width+b]=fill;
  }
 }
 else if(op>=7&&op<=10){for(unsigned n=0;n<16;++n)out[n]=memory[n%spec.width];}
 else {for(unsigned n=0;n<spec.width;++n)out[n]=memory[n];}
 return out;
}
}
