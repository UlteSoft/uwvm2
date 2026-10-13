#ifdef __cplusplus
extern "C" {
#endif
__attribute__((noinline)) int conditional_narrow(int flag) { volatile signed char a=-1,b=2; return flag ? a : b; }
__attribute__((noinline)) int conditional_boolean(int flag) { return flag ? 1 : 0; }
typedef short uwvm_short_alias;
__attribute__((noinline)) int bridge_char(int flag) { volatile char a=97,b=98; return flag ? a : b; }
__attribute__((noinline)) int bridge_schar(int flag) { volatile signed char a=-1,b=2; return flag ? a : b; }
__attribute__((noinline)) int bridge_uchar(int flag) { volatile unsigned char a=255,b=2; return flag ? a : b; }
__attribute__((noinline)) int bridge_short(int flag) { volatile short a=-1,b=2; return flag ? a : b; }
__attribute__((noinline)) int bridge_ushort(int flag) { volatile unsigned short a=65535,b=2; return flag ? a : b; }
__attribute__((noinline)) int bridge_alias(int flag) { volatile uwvm_short_alias a=-1,b=2; return flag ? a : b; }
typedef long uwvm_long_alias;
__attribute__((noinline)) long long rank_int(int flag) { volatile int a=-1,b=2; return flag ? a : b; }
__attribute__((noinline)) long long rank_uint(int flag) { volatile unsigned int a=3,b=2; return flag ? a : b; }
__attribute__((noinline)) long long rank_long(int flag) { volatile long a=-1,b=2; return flag ? a : b; }
__attribute__((noinline)) long long rank_ulong(int flag) { volatile unsigned long a=3,b=2; return flag ? a : b; }
__attribute__((noinline)) long long rank_ll(int flag) { volatile long long a=-1,b=2; return flag ? a : b; }
__attribute__((noinline)) long long rank_ull(int flag) { volatile unsigned long long a=3,b=2; return flag ? a : b; }
__attribute__((noinline)) long long rank_alias(int flag) { volatile uwvm_long_alias a=-1,b=2; return flag ? a : b; }
#ifdef __cplusplus
# define UWVM_TYPE_ASSERT(expr,type) static_assert(__is_same(decltype(expr),type),"independent Wasm integer rank witness")
#else
# define UWVM_TYPE_ASSERT(expr,type) _Static_assert(__builtin_types_compatible_p(__typeof__(expr),type),"independent Wasm integer rank witness")
#endif
UWVM_TYPE_ASSERT((int)1 + (long)2,long);
UWVM_TYPE_ASSERT((int)1 | (long)2,long);
UWVM_TYPE_ASSERT(1 ? (int)1 : (long)2,long);
UWVM_TYPE_ASSERT((long)1 << (int)2,long);
UWVM_TYPE_ASSERT(~(unsigned long)1,unsigned long);
UWVM_TYPE_ASSERT((long)1 + (long long)2,long long);
#if __SIZEOF_LONG__ > __SIZEOF_INT__
UWVM_TYPE_ASSERT((unsigned int)1 + (long)2,long);
#else
UWVM_TYPE_ASSERT((unsigned int)1 + (long)2,unsigned long);
#endif
#if __SIZEOF_LONG_LONG__ > __SIZEOF_LONG__
UWVM_TYPE_ASSERT((unsigned long)1 + (long long)2,long long);
#else
UWVM_TYPE_ASSERT((unsigned long)1 + (long long)2,unsigned long long);
#endif
#undef UWVM_TYPE_ASSERT
#ifdef __cplusplus
# define UWVM_SIZE_ASSERT(expr,extent) static_assert(sizeof(expr)==(extent),"independent unevaluated Wasm sizeof witness")
#else
# define UWVM_SIZE_ASSERT(expr,extent) _Static_assert(sizeof(expr)==(extent),"independent unevaluated Wasm sizeof witness")
#endif
UWVM_SIZE_ASSERT((int)1 + (long)2,sizeof(long));
UWVM_SIZE_ASSERT((unsigned long)1 + (long long)2,sizeof(long long));
UWVM_SIZE_ASSERT((short)1,sizeof(short));
UWVM_SIZE_ASSERT(+(short)1,sizeof(int));
UWVM_SIZE_ASSERT((float)1 + (double)2,sizeof(double));
UWVM_SIZE_ASSERT(1 / 0,sizeof(int));
UWVM_SIZE_ASSERT(1 << -1,sizeof(int));
UWVM_SIZE_ASSERT(1 ? (short)1 : (long long)2,sizeof(long long));
UWVM_SIZE_ASSERT(sizeof(int) + 1,sizeof(__SIZE_TYPE__));
UWVM_SIZE_ASSERT('(' + ')',sizeof(int));
#ifdef __cplusplus
UWVM_SIZE_ASSERT(true,sizeof(bool));
UWVM_SIZE_ASSERT(1 ? (short)1 : (short)2,sizeof(short));
#else
UWVM_SIZE_ASSERT((_Bool)1,sizeof(_Bool));
UWVM_SIZE_ASSERT(1 ? (short)1 : (short)2,sizeof(int));
#endif
#ifdef __cplusplus
# define UWVM_UNARY_ASSERT(expr,value) static_assert((expr)==(value),"independent unary sizeof witness")
#else
# define UWVM_UNARY_ASSERT(expr,value) _Static_assert((expr)==(value),"independent unary sizeof witness")
#endif
UWVM_UNARY_ASSERT(sizeof 1,sizeof(int));
UWVM_UNARY_ASSERT(sizeof -1,sizeof(int));
UWVM_UNARY_ASSERT(sizeof ~1,sizeof(int));
UWVM_UNARY_ASSERT(sizeof +1LL,sizeof(long long));
UWVM_UNARY_ASSERT(sizeof -1.0f,sizeof(float));
UWVM_UNARY_ASSERT(sizeof +1.0,sizeof(double));
#ifdef __cplusplus
UWVM_UNARY_ASSERT(sizeof '(' ,sizeof(char));
#else
UWVM_UNARY_ASSERT(sizeof '(' ,sizeof(int));
#endif
UWVM_UNARY_ASSERT(sizeof -1 + 2,sizeof(int)+2);
UWVM_UNARY_ASSERT(sizeof -1 * 2,sizeof(int)*2);
UWVM_UNARY_ASSERT(sizeof sizeof +1,sizeof(__SIZE_TYPE__));
UWVM_UNARY_ASSERT(sizeof + (short)1,sizeof(int));
UWVM_UNARY_ASSERT(sizeof - (unsigned char)1,sizeof(int));
UWVM_UNARY_ASSERT(sizeof 1 ? 2 : 3,2);
#ifdef __cplusplus
UWVM_UNARY_ASSERT(sizeof !1,sizeof(bool));
#else
UWVM_UNARY_ASSERT(sizeof !1,sizeof(int));
#endif
#undef UWVM_UNARY_ASSERT
#undef UWVM_SIZE_ASSERT
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
# define UWVM_ESCAPE_CASE(expr,value) static_assert((expr)==(value),"escape value witness"); static_assert(sizeof(expr)==sizeof(char),"CPP escape type witness");
#else
# define UWVM_ESCAPE_CASE(expr,value) _Static_assert((expr)==(value),"escape value witness"); _Static_assert(sizeof(expr)==sizeof(int),"C escape type witness");
#endif
#include "debug_source_character_escape_cases.h"
#undef UWVM_ESCAPE_CASE

#ifdef __cplusplus
# define UWVM_LOGICAL_ASSERT(expr) static_assert(__is_same(decltype(expr),bool),"independent CPP logical result witness")
#else
# define UWVM_LOGICAL_ASSERT(expr) _Static_assert(__builtin_types_compatible_p(__typeof__(expr),int),"independent C logical result witness")
#endif
UWVM_LOGICAL_ASSERT((char)1 && (short)2);
UWVM_LOGICAL_ASSERT((unsigned int)1 || (long)2);
UWVM_LOGICAL_ASSERT((long long)1 && (unsigned long long)2);
UWVM_LOGICAL_ASSERT((float)1 || (double)2);
UWVM_LOGICAL_ASSERT(0 && (1/0));
UWVM_LOGICAL_ASSERT(1 || (1/0));
#undef UWVM_LOGICAL_ASSERT
