#pragma once

#include <cstddef>

namespace fast_io::custom
{

/*
This is the user-editable default stack-buffer budget for fast_io print paths.

Users and downstream vendors may manually replace `value` below to tune the
default maximum number of bytes that stack-based print materialization may use.
The shipped default is selected from the target environment: tiny freestanding
and kernel-like builds get 256 bytes, wasm and non-GNU Linux get 4 KiB, Darwin
and unknown hosted Unix get 16 KiB, Windows gets 32 KiB, and GNU/Linux gets
64 KiB.  A value of 0 disables the default stack budget for paths that query
this customization point.

ODR note:
Do not implement this knob as a per-translation-unit macro.  Macro-controlled
inline functions can make two TUs emit the same inline/template entity with
different compile-time constants, which is an ODR/IFNDR trap.

This customization point is intentionally a template value consumed by
stack/impl.h to form:

    default_print_stack_policy = print_stack_policy<value>

The selected byte count is therefore encoded into the policy type and into the
template specializations that depend on it.  When the value is changed for a
build, stack-budget-sensitive helper instantiations get a different type identity
and different mangled symbols instead of reusing the same specialization with a
different body.  All TUs that include the same edited header still see the same
token sequence and the same name-lookup results, so the ordinary header-only ODR
requirements are preserved.

Different libraries or binaries built with different edited values may therefore
coexist without requiring their print stack policy instantiations to be shared.
The tradeoff is possible code-size growth from separate template instantiations.
*/
template <typename = void>
struct print_stack_buffer_default_max_bytes
{
	static inline constexpr ::std::size_t value{
#if defined(__KERNEL__) || defined(_KERNEL) || defined(_KERNEL_MODE)
		256u
#elif defined(__EMSCRIPTEN__) || defined(__wasm32__) || defined(__wasm64__) || defined(__wasm__)
		4u * 1024u
#elif !defined(__STDC_HOSTED__) || (__STDC_HOSTED__ == 0)
		256u
#elif defined(_WIN32) || defined(__CYGWIN__)
		32u * 1024u
#elif defined(__APPLE__) && defined(__MACH__)
		16u * 1024u
#elif defined(__linux__) && defined(__gnu_linux__)
		64u * 1024u
#elif defined(__linux__)
		4u * 1024u
#elif defined(__unix__) || defined(__unix) || defined(unix)
		16u * 1024u
#else
		16u * 1024u
#endif
	};
};

} // namespace fast_io::custom
