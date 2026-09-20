#pragma push_macro("erase")
#undef erase

#pragma push_macro("interface")
#undef interface

#pragma push_macro("max")
#undef max

#pragma push_macro("min")
#undef min

#pragma push_macro("move")
#undef move

#pragma push_macro("new")
#if __GNUC__ >= 16
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wkeyword-macro"
#undef new
#pragma GCC diagnostic pop
#else
#undef new
#endif

#pragma push_macro("refresh")
#undef refresh


#pragma push_macro("FAST_IO_DLLIMPORT")
#undef FAST_IO_DLLIMPORT
#if defined(_MSC_VER) && !defined(__clang__)
#define FAST_IO_DLLIMPORT __declspec(dllimport)
#elif __has_cpp_attribute(__gnu__::__dllimport__) && !defined(__WINE__) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
#define FAST_IO_DLLIMPORT [[__gnu__::__dllimport__]]
#else
#define FAST_IO_DLLIMPORT
#endif

#pragma push_macro("FAST_IO_DLL_DLLIMPORT")
#undef FAST_IO_DLL_DLLIMPORT
#if defined(_DLL) && !defined(__WINE__)
#define FAST_IO_DLL_DLLIMPORT FAST_IO_DLLIMPORT
#else
#define FAST_IO_DLL_DLLIMPORT
#endif

#pragma push_macro("FAST_IO_STDCALL")
#undef FAST_IO_STDCALL
#if defined(_MSC_VER) && (!__has_cpp_attribute(__gnu__::__stdcall__) && !defined(__WINE__))
#define FAST_IO_STDCALL __stdcall
#elif (__has_cpp_attribute(__gnu__::__stdcall__) && !defined(__WINE__))
#define FAST_IO_STDCALL __attribute__((__stdcall__))
#else
#define FAST_IO_STDCALL
#endif

#pragma push_macro("FAST_IO_WINSTDCALL")
#undef FAST_IO_WINSTDCALL
#if defined(_MSC_VER) && (!__has_cpp_attribute(__gnu__::__stdcall__) && !defined(__WINE__))
#define FAST_IO_WINSTDCALL __stdcall
#elif (__has_cpp_attribute(__gnu__::__stdcall__) && !defined(__WINE__))
#define FAST_IO_WINSTDCALL __attribute__((__stdcall__))
#else
#define FAST_IO_WINSTDCALL
#endif

#pragma push_macro("FAST_IO_WINSTDCALL_RENAME")
#undef FAST_IO_WINSTDCALL_RENAME
#if defined(__clang__) || defined(__GNUC__)
#if defined(_M_HYBRID)
#define FAST_IO_WINSTDCALL_RENAME(name, count) __asm__("#" #name "@" #count)
#elif defined(__arm64ec__) || defined(_M_ARM64EC)
#define FAST_IO_WINSTDCALL_RENAME(name, count) __asm__("#" #name)
#elif SIZE_MAX <= UINT_LEAST32_MAX && (defined(__x86__) || defined(_M_IX86) || defined(__i386__))
#if !defined(__clang__)
#define FAST_IO_WINSTDCALL_RENAME(name, count) __asm__(#name "@" #count)
#else
#define FAST_IO_WINSTDCALL_RENAME(name, count) __asm__("_" #name "@" #count)
#endif
#else
#define FAST_IO_WINSTDCALL_RENAME(name, count) __asm__(#name)
#endif
#else
#define FAST_IO_WINSTDCALL_RENAME(name, count)
#endif

#pragma push_macro("FAST_IO_WINCDECL")
#undef FAST_IO_WINCDECL
#if defined(_MSC_VER) && (!__has_cpp_attribute(__gnu__::__cdecl__) && !defined(__WINE__))
#define FAST_IO_WINCDECL __cdecl
#elif (__has_cpp_attribute(__gnu__::__cdecl__) && !defined(__WINE__))
#define FAST_IO_WINCDECL __attribute__((__cdecl__))
#else
#define FAST_IO_WINCDECL
#endif

#pragma push_macro("FAST_IO_WINCDECL_RENAME")
#undef FAST_IO_WINCDECL_RENAME
#if defined(__clang__) || defined(__GNUC__)
#if defined(_M_HYBRID)
#define FAST_IO_WINCDECL_RENAME(name, count) __asm__("#" #name "@" #count)
#elif defined(__arm64ec__) || defined(_M_ARM64EC)
#define FAST_IO_WINCDECL_RENAME(name, count) __asm__("#" #name)
#elif SIZE_MAX <= UINT_LEAST32_MAX && (defined(__x86__) || defined(_M_IX86) || defined(__i386__))
#if !defined(__clang__)
#define FAST_IO_WINCDECL_RENAME(name, count) __asm__(#name)
#else
#define FAST_IO_WINCDECL_RENAME(name, count) __asm__("_" #name)
#endif
#else
#define FAST_IO_WINCDECL_RENAME(name, count) __asm__(#name)
#endif
#else
#define FAST_IO_WINCDECL_RENAME(name, count)
#endif

#pragma push_macro("FAST_IO_WINFASTCALL")
#undef FAST_IO_WINFASTCALL
#if defined(_MSC_VER) && (!__has_cpp_attribute(__gnu__::__fastcall__) && !defined(__WINE__))
#define FAST_IO_WINFASTCALL __fastcall
#elif (__has_cpp_attribute(__gnu__::__fastcall__) && !defined(__WINE__))
#define FAST_IO_WINFASTCALL __attribute__((__fastcall__))
#else
#define FAST_IO_WINFASTCALL
#endif

#pragma push_macro("FAST_IO_WINFASTCALL_RENAME")
#undef FAST_IO_WINFASTCALL_RENAME
#if defined(__clang__) || defined(__GNUC__)
#if defined(_M_HYBRID)
#define FAST_IO_WINFASTCALL_RENAME(name, count) __asm__("#" #name "@" #count)
#elif defined(__arm64ec__) || defined(_M_ARM64EC)
#define FAST_IO_WINFASTCALL_RENAME(name, count) __asm__("#" #name)
#elif SIZE_MAX <= UINT_LEAST32_MAX && (defined(__x86__) || defined(_M_IX86) || defined(__i386__))
#if !defined(__clang__)
#define FAST_IO_WINFASTCALL_RENAME(name, count) __asm__("@" #name "@" #count)
#else
#define FAST_IO_WINFASTCALL_RENAME(name, count) __asm__("_@" #name "@" #count)
#endif
#else
#define FAST_IO_WINFASTCALL_RENAME(name, count) __asm__(#name)
#endif
#else
#define FAST_IO_WINFASTCALL_RENAME(name, count)
#endif

#pragma push_macro("FAST_IO_GNU_CONST")
#undef FAST_IO_GNU_CONST
#if __has_cpp_attribute(__gnu__::__const__)
#define FAST_IO_GNU_CONST [[__gnu__::__const__]]
#else
#define FAST_IO_GNU_CONST
#endif

#pragma push_macro("FAST_IO_GNU_ALWAYS_INLINE")
#undef FAST_IO_GNU_ALWAYS_INLINE
#if __has_cpp_attribute(__gnu__::__always_inline__)
#define FAST_IO_GNU_ALWAYS_INLINE [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
#define FAST_IO_GNU_ALWAYS_INLINE [[msvc::forceinline]]
#else
#define FAST_IO_GNU_ALWAYS_INLINE
#endif

#pragma push_macro("FAST_IO_GNU_ARTIFICIAL")
#undef FAST_IO_GNU_ARTIFICIAL
#if __has_cpp_attribute(__gnu__::__artificial__)
#define FAST_IO_GNU_ARTIFICIAL [[__gnu__::__artificial__]]
#else
#define FAST_IO_GNU_ARTIFICIAL
#endif


#pragma push_macro("FAST_IO_GNU_ALWAYS_INLINE_ARTIFICIAL")
#undef FAST_IO_GNU_ALWAYS_INLINE_ARTIFICIAL
#define FAST_IO_GNU_ALWAYS_INLINE_ARTIFICIAL FAST_IO_GNU_ALWAYS_INLINE FAST_IO_GNU_ARTIFICIAL

#pragma push_macro("FAST_IO_GNU_ALWAYS_INLINE_ARTIFICIAL_CONST")
#undef FAST_IO_GNU_ALWAYS_INLINE_ARTIFICIAL_CONST
#define FAST_IO_GNU_ALWAYS_INLINE_ARTIFICIAL_CONST \
	FAST_IO_GNU_ALWAYS_INLINE [[nodiscard]] FAST_IO_GNU_ARTIFICIAL FAST_IO_GNU_CONST

#pragma push_macro("FAST_IO_GNU_MALLOC")
#undef FAST_IO_GNU_MALLOC
#if __has_cpp_attribute(__gnu__::__malloc__)
#define FAST_IO_GNU_MALLOC [[__gnu__::__malloc__]]
#else
#define FAST_IO_GNU_MALLOC
#endif

#pragma push_macro("FAST_IO_GNU_RETURNS_NONNULL")
#if __has_cpp_attribute(__gnu__::__returns_nonnull__)
#undef FAST_IO_GNU_RETURNS_NONNULL
#define FAST_IO_GNU_RETURNS_NONNULL [[__gnu__::__returns_nonnull__]]
#else
#define FAST_IO_GNU_RETURNS_NONNULL
#endif

#pragma push_macro("FAST_IO_ASSERT")
#undef FAST_IO_ASSERT
/*
Internal assert macros for fuzzing fast_io.
*/
#if defined(FAST_IO_DEBUG)
#if defined(_MSC_VER) && !defined(__clang__)
#define FAST_IO_ASSERT(x)                           \
	if (!__builtin_is_constant_evaluated() && !(x)) \
	::std::abort()
#else
#define FAST_IO_ASSERT(x)                           \
	if (!__builtin_is_constant_evaluated() && !(x)) \
	__builtin_trap()
#endif
#else
#define FAST_IO_ASSERT(x) ((void)0)
#endif

#pragma push_macro("FAST_IO_HAS_BUILTIN")
#undef FAST_IO_HAS_BUILTIN
#ifdef __has_builtin
#define FAST_IO_HAS_BUILTIN(...) __has_builtin(__VA_ARGS__)
#else
#define FAST_IO_HAS_BUILTIN(...) 0
#endif

#pragma push_macro("FAST_IO_HAS_ATTRIBUTE")
#undef FAST_IO_HAS_ATTRIBUTE
#ifdef __has_cpp_attribute
#define FAST_IO_HAS_ATTRIBUTE(...) __has_cpp_attribute(__VA_ARGS__)
#else
#define FAST_IO_HAS_ATTRIBUTE(...) 0
#endif

#pragma push_macro("FAST_IO_HAS_STATIC_CALL_OPERATOR_IN_LANGUAGE_MODE")
#undef FAST_IO_HAS_STATIC_CALL_OPERATOR_IN_LANGUAGE_MODE
// Clang exposes __cpp_static_call_operator in C++20 as an extension, but strict
// C++20 rejects the syntax.  The language-mode half of this gate prevents a
// feature-test macro from admitting syntax that the selected standard forbids.
#if defined(__cpp_static_call_operator) && __cpp_static_call_operator >= 202207L && \
	((defined(_MSVC_LANG) && _MSVC_LANG > 202002L) ||                              \
	 (!defined(_MSVC_LANG) && __cplusplus > 202002L))
#define FAST_IO_HAS_STATIC_CALL_OPERATOR_IN_LANGUAGE_MODE 1
#else
#define FAST_IO_HAS_STATIC_CALL_OPERATOR_IN_LANGUAGE_MODE 0
#endif

#pragma push_macro("FAST_IO_IF_CONSTEVAL")
#undef FAST_IO_IF_CONSTEVAL
// Feature-macro presence is the two-way syntax gate. Clang 14's experimental
// C++2b implementation is the narrow exception: both `if consteval` and its
// library query fold runtime constexpr calls through the constant arm, while
// the compiler builtin retains the required call-site semantics.
#if defined(__cpp_if_consteval) && defined(__clang__) && \
	__clang_major__ == 14
#define FAST_IO_IF_CONSTEVAL \
	if ([]() constexpr noexcept { return __builtin_is_constant_evaluated(); }())
#elif defined(__cpp_if_consteval)
#define FAST_IO_IF_CONSTEVAL if consteval
#else
// The lambda keeps GCC from diagnosing the query as tautologically false when
// an otherwise shared helper is not itself constexpr.  It folds completely and
// still asks the standard C++20 query in the caller's evaluation context.
#define FAST_IO_IF_CONSTEVAL \
	if ([]() constexpr noexcept { return ::std::is_constant_evaluated(); }())
#endif

#pragma push_macro("FAST_IO_IF_NOT_CONSTEVAL")
#undef FAST_IO_IF_NOT_CONSTEVAL
// Keep the runtime half symmetric with FAST_IO_IF_CONSTEVAL so call sites never
// need to duplicate feature-test branches or rely on a C++23 extension in C++20.
#if defined(__cpp_if_consteval) && defined(__clang__) && \
	__clang_major__ == 14
#define FAST_IO_IF_NOT_CONSTEVAL \
	if (![]() constexpr noexcept { return __builtin_is_constant_evaluated(); }())
#elif defined(__cpp_if_consteval)
#define FAST_IO_IF_NOT_CONSTEVAL if !consteval
#else
#define FAST_IO_IF_NOT_CONSTEVAL \
	if (![]() constexpr noexcept { return ::std::is_constant_evaluated(); }())
#endif

#pragma push_macro("FAST_IO_ASSUME")
#undef FAST_IO_ASSUME
// Clang advertises the C++23 assume attribute while compiling as C++20, where
// using it is still a pedantic error.  Require both a post-C++20 language mode
// and attribute support, then use the compiler builtin when strict C++20 has
// no standard spelling.  Omitting the hint is always semantics-preserving.
#if (((defined(_MSVC_LANG) && _MSVC_LANG > 202002L) || \
	  (!defined(_MSVC_LANG) && __cplusplus > 202002L)) && \
	 FAST_IO_HAS_ATTRIBUTE(assume))
#define FAST_IO_ASSUME(...) [[assume(__VA_ARGS__)]]
#elif FAST_IO_HAS_BUILTIN(__builtin_assume)
#define FAST_IO_ASSUME(...) __builtin_assume(__VA_ARGS__)
#else
// Keep expressions type-checked and their local inputs marked as used without
// evaluating them or adding a runtime branch when no assumption facility exists.
#define FAST_IO_ASSUME(...) ((void)sizeof(static_cast<bool>(__VA_ARGS__)))
#endif

#pragma push_macro("FAST_IO_CPP_RTTI")
#undef FAST_IO_CPP_RTTI
#if defined(_MSC_VER) && !defined(__clang__)
#if __cpp_rtti >= 199711L && _HAS_RTTI
#define FAST_IO_CPP_RTTI
#endif
#else
#if __cpp_rtti >= 199711L
#define FAST_IO_CPP_RTTI
#endif
#endif

#pragma push_macro("FAST_IO_CPP_EXCEPTIONS")
#undef FAST_IO_CPP_EXCEPTIONS
#if defined(_MSC_VER) && !defined(__clang__)
#if __cpp_exceptions >= 199711L && _HAS_EXCEPTIONS
#define FAST_IO_CPP_EXCEPTIONS
#endif
#else
#if __cpp_exceptions >= 199711L
#define FAST_IO_CPP_EXCEPTIONS
#endif
#endif

#pragma push_macro("FAST_IO_HERBCEPTIONS_THROWS")
#undef FAST_IO_HERBCEPTIONS_THROWS
#pragma push_macro("FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT")
#undef FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT
#pragma push_macro("FAST_IO_HERBCEPTIONS_NOTHROWS")
#undef FAST_IO_HERBCEPTIONS_NOTHROWS
#pragma push_macro("FAST_IO_HERBCEPTIONS_NOEXCEPT")
#undef FAST_IO_HERBCEPTIONS_NOEXCEPT

/*
Herbceptions and traditional C++ exceptions are orthogonal effects.  A
no-failure proof must therefore inspect both effects: `throws(expr)` observes
the deterministic error channel, while `noexcept(expr)` excludes a possible
unwinding exception.  On standard compilers the latter remains the complete
query.

`_THROWS_OR_NOEXCEPT` receives two logically independent proofs.  Its first
argument is true exactly when the deterministic channel may fail; its second
argument is true exactly when the ordinary expression cannot unwind.  Keeping
both propositions explicit is required because neither one implies the other.
In Herbception mode either possible failure activates the standard
`throws(bool)` channel: the language converts a legacy exception escaping an
active basic-throws function into `std::error`, whereas `throws(false)` is
identical to `noexcept(true)` and must only be selected after *both* effects
have been excluded.  Standard toolchains retain the established
conditional-noexcept contract from the second proposition.

Most protocol concepts already require the traditional expression to be
`noexcept`; `_NOTHROWS` adds only the orthogonal deterministic-channel proof
and expands to `true` elsewhere, avoiding duplicate constraint instantiation
on GCC and ordinary Clang. `_NOEXCEPT` is the combined query used where both
channels must be classified by one expression.

The extension currently exposes no `__cpp_*` revision macro.  Its compiler-
provided `__HERBCEPTIONS__` gate is consequently paired with the required
runtime header in fast_io_concept.h instead of guessing from a Clang version.
*/
#if defined(__HERBCEPTIONS__)
#define FAST_IO_HERBCEPTIONS_THROWS throws
#define FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(herb_may_fail, ...) \
	throws((herb_may_fail) || !(__VA_ARGS__))
#define FAST_IO_HERBCEPTIONS_NOTHROWS(...) (!throws((__VA_ARGS__)))
#define FAST_IO_HERBCEPTIONS_NOEXCEPT(...) (!throws((__VA_ARGS__)) && noexcept((__VA_ARGS__)))
#else
#define FAST_IO_HERBCEPTIONS_THROWS
#define FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(herb_may_fail, ...) noexcept((__VA_ARGS__))
#define FAST_IO_HERBCEPTIONS_NOTHROWS(...) true
#define FAST_IO_HERBCEPTIONS_NOEXCEPT(...) noexcept((__VA_ARGS__))
#endif
