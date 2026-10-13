// Independent target compiler type witnesses and genuine embedded DWARF.
#ifdef __cplusplus
# define TYPE(expr,T) static_assert(__is_same(decltype(expr),T),"Wasm size_t ABI/type witness")
#else
# define TYPE(expr,T) _Static_assert(__builtin_types_compatible_p(__typeof__(expr),T),"Wasm size_t ABI/type witness")
#endif
TYPE(sizeof(int),unsigned long);
TYPE(sizeof(1+2),unsigned long);
TYPE(sizeof(sizeof(int)),unsigned long);
TYPE((__SIZE_TYPE__)1,unsigned long);
TYPE(sizeof(int)+(long)1,unsigned long);
TYPE(sizeof(int)+(unsigned int)1,unsigned long);
TYPE(1?sizeof(int):(long)1,unsigned long);
TYPE(0?sizeof(int):(long)1,unsigned long);
TYPE(sizeof(int)<<1,unsigned long);
TYPE(~(__SIZE_TYPE__)0,unsigned long);
#if __SIZEOF_POINTER__==4
TYPE(sizeof(int)+(long long)1,long long);
TYPE(1?sizeof(int):(long long)1,long long);
#else
TYPE(sizeof(int)+(long long)1,unsigned long long);
TYPE(1?sizeof(int):(long long)1,unsigned long long);
#endif
TYPE(sizeof(int)+(unsigned long long)1,unsigned long long);
#undef TYPE
__attribute__((noinline)) unsigned long long size_rank(int flag)
{
 volatile __SIZE_TYPE__ a=4;volatile long b=1;volatile long long c=1;
 return flag?(a+b):(a+c);
}
