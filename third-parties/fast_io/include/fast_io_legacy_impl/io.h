#pragma once

/*
 * Public IO scenario facade (IO level).
 *
 * `fast_io::io::print`, `println`, `perr`, `perrln`, `panic`, `panicln`, and
 * `scan` add user-facing operation semantics around the generic freestanding
 * engines. This level distinguishes an explicit stream from a default native
 * sink, checks character-domain diagnostics, owns newline/error/cold/terminate
 * behavior, and converts reporting scan results to the documented public
 * result. It then normalizes the stream exactly once and enters the same
 * pre-normalization/decay print core used by
 * `operations::print_freestanding`, or the corresponding scan decay operation.
 *
 * The facade does not implement value formatting or raw device transfer.
 * Printable/scannable objects are normalized by the IO protocols, complete
 * operation strategy is selected by the freestanding engines, and final
 * read/write capability is supplied by device CPOs.
 */

#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
#include "defined_types.h"
#endif

namespace fast_io
{

inline namespace io
{

// All explicit-output print facades below establish the same operation
// lifetime boundary. A borrowed/lvalue stream is merely normalized once; an
// opted-in owning temporary is checked-finished only after successful emission.
// This applies uniformly to print/println, diagnostic output, and debug output.

/** @brief Prints arguments to an explicit stream or the default output fallback. */
template <typename T, typename... Args>
// GCC 11--16 leave this facade outlined at -O3. That boundary hides source-expression constants from the core
// __builtin_constant_p gate and adds a public-wrapper call to the unknown-value path. Removing only this marker makes
// every tested constant int/double obuffer wrapper call the runtime formatter; on GCC 11 and 12 it also slows a runtime
// int/double obuffer loop by 10--12%. This is a measured compile-time tradeoff: at sixteen call sites forcing costs
// roughly 1.5--2.8x compilation across GCC 11--16, but runtime text changes by only about -1.5% to +3% because the
// compiler still shares the lower continuation. An explicitly noinline continuation was rejected because it reproduced
// an approximately 12% runtime regression. Clang 17--23 performs that direct-scalar facade decision unaided. A separate
// recursive dynamic-star condition audit proves that this same facade is jointly necessary in the six-edge chain on
// Clang 21--23: deleting it restores a reachable proxy/native formatter graph after the format level has selected the
// active IO record. Clang 16--20 still fail with the complete chain and add 6.8--22.1 KiB of text, so the Clang range
// starts at the first proved endpoint and remains future-open.
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void print(T &&t, Args &&...args)
{
	constexpr bool device_and_type_ok{
		::fast_io::operations::defines::print_freestanding_okay_for_line<false, T, Args...>};
	if constexpr (device_and_type_ok)
	{
		// Emit to the explicit device under one success-only output guard.
		::fast_io::operations::basic_output_operation_guard<T &&> guard{t};
		decltype(auto) outref = guard.ref();
		using normalized_output = ::std::remove_cvref_t<decltype(outref)>;
		using char_type = typename normalized_output::output_char_type;
		if constexpr (::fast_io::details::decay::print_fixed_public_run_available<
						 normalized_output, char_type, decltype(args)...>())
		{
			::fast_io::details::decay::print_fixed_public_run<false, normalized_output, char_type>(outref, args...);
		}
		else
		{
			::fast_io::operations::decay::
				print_freestanding_compiler_constant_pre_normalization<false>(
					outref, args...);
		}
		guard.commit();
	}
	else
	{
		// Diagnose the explicit form or reinterpret the first argument as payload.
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES)) &&                                     \
	__has_include(<stdio.h>)
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		if constexpr (device_ok)
		{
			// The first argument is a device, so only argument printability failed.
			if constexpr (::fast_io::details::has_raw_print_arg<Args...>)
			{
				// Produce the dedicated diagnostic for forbidden raw print arguments.
				::fast_io::details::print_raw_static_assert<Args...>();
			}
			else
			{
				// Report the general explicit-device printable constraint failure.
				static_assert(device_and_type_ok,
							  "some types are not printable for print on the provided output stream");
			}
		}
		else
		{
			// Treat all arguments as payload for the hosted default output stream.
			constexpr bool type_ok{::fast_io::operations::defines::print_freestanding_params_okay<char, T, Args...>};
			if constexpr (type_ok)
			{
				// Continue through the default-output source-normalization boundary.
				::fast_io::details::print_after_source_pre_normalization<false>(
					t, args...);
			}
			else
			{
				// Diagnose invalid payload types without touching the default device.
				if constexpr (::fast_io::details::has_raw_print_arg<T, Args...>)
				{
					// Prefer the more actionable raw-argument diagnostic.
					::fast_io::details::print_raw_static_assert<T, Args...>();
				}
				else
				{
					// Report the default-output printable constraint failure.
				// clang-format off
static_assert(type_ok, "some types are not printable for print on default C's stdout");
				// clang-format on
				}
			}
		}
#else
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		// clang-format off
static_assert(device_ok, "freestanding environment must provide IO device for print");
static_assert(device_and_type_ok, "some types are not printable for print");
		// clang-format on
#endif
	}
}

/** @brief Prints arguments followed by a newline with guarded output lifetime. */
template <typename T, typename... Args>
// Keep the line-terminating facade on the same measured GCC policy as print. GCC 11--16 otherwise outline it and lose
// both caller-expression constant discovery and the small unknown-value hot path, whereas Clang 17--23 needs no
// override. The lower continuation remains compiler-shareable, which bounds text growth at larger call-site counts.
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void println(T &&t, Args &&...args)
{
	constexpr bool device_and_type_ok{
		::fast_io::operations::defines::print_freestanding_okay_for_line<true, T, Args...>};
	if constexpr (device_and_type_ok)
	{
		// Emit the explicit line record under one success-only output guard.
		::fast_io::operations::basic_output_operation_guard<T &&> guard{t};
		decltype(auto) outref = guard.ref();
		using normalized_output = ::std::remove_cvref_t<decltype(outref)>;
		using char_type = typename normalized_output::output_char_type;
		if constexpr (::fast_io::details::decay::print_fixed_public_run_available<
						 normalized_output, char_type, decltype(args)...>())
		{
			::fast_io::details::decay::print_fixed_public_run<true, normalized_output, char_type>(outref, args...);
		}
		else
		{
			::fast_io::operations::decay::
				print_freestanding_compiler_constant_pre_normalization<true>(
					outref, args...);
		}
		guard.commit();
	}
	else
	{
		// Diagnose the explicit form or select hosted default-output semantics.
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES)) &&                                     \
	__has_include(<stdio.h>)
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		if constexpr (device_ok)
		{
			// The explicit device is valid, leaving only payload-type failure.
			if constexpr (::fast_io::details::has_raw_print_arg<Args...>)
			{
				// Produce the dedicated forbidden-raw-argument diagnostic.
				::fast_io::details::print_raw_static_assert<Args...>();
			}
			else
			{
				// Report the general explicit-device printable failure.
				static_assert(device_and_type_ok,
							  "some types are not printable for println on the provided output stream");
			}
		}
		else
		{
			// Treat the first argument as payload for hosted default output.
			constexpr bool type_ok{::fast_io::operations::defines::print_freestanding_params_okay<char, T, Args...>};
			if constexpr (type_ok)
			{
				// Continue through default-output normalization with line semantics.
				::fast_io::details::print_after_source_pre_normalization<true>(
					t, args...);
			}
			else
			{
				// Diagnose payloads before any default device is observed.
				if constexpr (::fast_io::details::has_raw_print_arg<T, Args...>)
				{
					// Prefer the dedicated raw-argument constraint diagnostic.
					::fast_io::details::print_raw_static_assert<T, Args...>();
				}
				else
				{
					// Report the default-output line-printable failure.
					static_assert(type_ok, "some types are not printable for print on default C's stdout");
				}
			}
		}
#else
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		// clang-format off
static_assert(device_ok, "freestanding environment must provide IO device for println");
static_assert(device_and_type_ok, "some types are not printable for println");
		// clang-format on
#endif
	}
}

/** @brief Prints diagnostics without a newline under guarded output lifetime. */
template <typename T, typename... Args>
inline constexpr void perr(T &&t, Args &&...args)
{
	constexpr bool device_and_type_ok{
		::fast_io::operations::defines::print_freestanding_okay_for_line<false, T, Args...>};
	if constexpr (device_and_type_ok)
	{
		// Emit to the explicit diagnostic device under one output guard.
		::fast_io::operations::basic_output_operation_guard<T &&> guard{t};
		decltype(auto) outref = guard.ref();
		::fast_io::operations::decay::
			print_freestanding_compiler_constant_pre_normalization_cold<false>(
				outref, args...);
		guard.commit();
	}
	else
	{
		// Diagnose the explicit form or select the hosted native error sink.
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && !defined(_LIBCPP_FREESTANDING) && \
	  !defined(__AVR__)) ||                                                                                            \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		if constexpr (device_ok)
		{
			// The explicit device is valid, so diagnose its payload arguments.
			if constexpr (::fast_io::details::has_raw_print_arg<Args...>)
			{
				// Produce the dedicated forbidden-raw-argument diagnostic.
				::fast_io::details::print_raw_static_assert<Args...>();
			}
			else
			{
				// Report the general explicit diagnostic-print constraint failure.
				static_assert(device_and_type_ok,
							  "some types are not printable for perr on the provided output stream");
			}
		}
		else
		{
			// Treat every argument as payload for the native error sink.
			constexpr bool type_ok{::fast_io::operations::defines::print_freestanding_params_okay<char, T, Args...>};
			if constexpr (type_ok)
			{
				// Continue through the cold default-error normalization path.
				::fast_io::details::perr_after_source_pre_normalization<false>(
					t, args...);
			}
			else
			{
				// Diagnose invalid payload before obtaining the error sink.
				if constexpr (::fast_io::details::has_raw_print_arg<T, Args...>)
				{
					// Prefer the dedicated raw-argument diagnostic.
					::fast_io::details::print_raw_static_assert<T, Args...>();
				}
				else
				{
					// Report the native-error printable constraint failure.
				// clang-format off
static_assert(type_ok, "some types are not printable for perr on native err");
				// clang-format on
				}
			}
		}
#else
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		// clang-format off
static_assert(device_ok, "freestanding environment must provide IO device for perr");
static_assert(device_and_type_ok, "some types are not printable for perr");
		// clang-format on
#endif
	}
}

/** @brief Prints diagnostics plus a newline under guarded output lifetime. */
template <typename T, typename... Args>
inline constexpr void perrln(T &&t, Args &&...args)
{
	constexpr bool device_and_type_ok{
		::fast_io::operations::defines::print_freestanding_okay_for_line<true, T, Args...>};
	if constexpr (device_and_type_ok)
	{
		// Emit the explicit diagnostic line under one output guard.
		::fast_io::operations::basic_output_operation_guard<T &&> guard{t};
		decltype(auto) outref = guard.ref();
		::fast_io::operations::decay::
			print_freestanding_compiler_constant_pre_normalization_cold<true>(
				outref, args...);
		guard.commit();
	}
	else
	{
		// Diagnose the explicit form or select the hosted native error sink.
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && !defined(_LIBCPP_FREESTANDING) && \
	  !defined(__AVR__)) ||                                                                                            \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		if constexpr (device_ok)
		{
			// The explicit device is valid, so diagnose only its payload.
			if constexpr (::fast_io::details::has_raw_print_arg<Args...>)
			{
				// Produce the dedicated forbidden-raw-argument diagnostic.
				::fast_io::details::print_raw_static_assert<Args...>();
			}
			else
			{
				// Report the general explicit diagnostic-line constraint failure.
				static_assert(device_and_type_ok,
							  "some types are not printable for perrln on the provided output stream");
			}
		}
		else
		{
			// Treat every argument as payload for the native error sink.
			constexpr bool type_ok{::fast_io::operations::defines::print_freestanding_params_okay<char, T, Args...>};
			if constexpr (type_ok)
			{
				// Continue through cold default-error normalization with a newline.
				::fast_io::details::perr_after_source_pre_normalization<true>(
					t, args...);
			}
			else
			{
				// Diagnose invalid payload before obtaining the error sink.
				if constexpr (::fast_io::details::has_raw_print_arg<T, Args...>)
				{
					// Prefer the dedicated raw-argument diagnostic.
					::fast_io::details::print_raw_static_assert<T, Args...>();
				}
				else
				{
					// Report the native-error line-printable constraint failure.
				// clang-format off
static_assert(type_ok, "some types are not printable for perrln on native err");
				// clang-format on
				}
			}
		}
#else
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		// clang-format off
static_assert(device_ok, "freestanding environment must provide IO device for perrln");
static_assert(device_and_type_ok, "some types are not printable for perrln");
		// clang-format on
#endif
	}
}

namespace panic_details
{

/// @brief Owns panic's exact historical perr/catch/terminate operation in a dedicated cold continuation.
/// @details This is a semantic fallback boundary, not a promise that an outlined public panic wrapper adds no call
///          frame. Current compilers may retain both functions when their noreturn/cold inline threshold is zero.
template <bool line, typename... Args>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
[[noreturn]] inline constexpr void fallback(Args &&...args) noexcept
{
#ifdef __cpp_exceptions
	try
	{
#endif
		if constexpr (sizeof...(Args) == 0u)
		{
			// A hosted source-free panic still owns one diagnostic print record.
			// The default error sink is normalized once, after which the print
			// level invokes an exact empty-status operation, emits a requested
			// newline, or discards an unobservable non-line record. A freestanding
			// build has no library-owned default error sink and terminates directly.
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
			::fast_io::details::perr_after_source_pre_normalization<line>();
#endif
		}
		else if constexpr (line)
		{
			// Route a nonempty line panic through the diagnostic newline facade.
			::fast_io::io::perrln(::std::forward<Args>(args)...);
		}
		else
		{
			// Route a nonempty non-line panic through the diagnostic facade.
			::fast_io::io::perr(::std::forward<Args>(args)...);
		}
#ifdef __cpp_exceptions
	}
	catch (...)
	{
		// Panic suppresses diagnostic failures because termination is unconditional.
	}
#endif
	::fast_io::fast_terminate();
}

} // namespace panic_details

/// @brief Prints an optional diagnostic through perr and terminates.
/// @details The speculative source gate is deliberately ordinary-inline. Current Clang and GCC may outline a noreturn
///          call at an effective zero inline threshold, in which case caller-literal `__builtin_constant_p` evidence is
///          unavailable and the exact cold fallback is selected. Do not force this diagnostic front door inline merely
///          to recover that optional strategy; the structural gate remains here for callers/targets which inline it
///          naturally and for future source representations carrying compile-time storage in their type. Consequently
///          this interface promises neither caller-literal folding nor a single generated call frame on current tools.
///          A source-free call still enters the same cold print-level fallback: ordinary native error output ignores
///          the empty record, while a future default sink with an exact zero-argument status operation may observe it.
template <typename... Args>
[[noreturn]] inline constexpr void panic(Args &&...args) noexcept
{
	if constexpr (sizeof...(Args) == 0u)
	{
		// A source-free panic uses the dedicated default diagnostic fallback.
		::fast_io::io::panic_details::fallback<false>();
	}
	else
	{
		// Try the constant diagnostic arm before the exact cold fallback.
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
#ifdef __cpp_exceptions
		try
		{
			// Isolate optional diagnostic work from the termination guarantee.
#endif
			if (::fast_io::details::
					panic_try_compiler_constant_pre_normalization<false, Args...>(args...))
			{
				// Successful emission terminates before fallback can repeat it.
				::fast_io::fast_terminate();
			}
#ifdef __cpp_exceptions
		}
		catch (...)
		{
			// Convert any diagnostic exception to immediate termination.
			::fast_io::fast_terminate();
		}
#endif
#endif
		::fast_io::io::panic_details::fallback<false>(
			::std::forward<Args>(args)...);
	}
}

/** @brief Prints a line diagnostic if possible and unconditionally terminates. */
template <typename... Args>
	requires(sizeof...(Args) != 0)
[[noreturn]] inline constexpr void panicln(Args &&...args) noexcept
{
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
#ifdef __cpp_exceptions
	try
	{
		// Isolate optional line-diagnostic work from the termination guarantee.
#endif
		if (::fast_io::details::
				panic_try_compiler_constant_pre_normalization<true, Args...>(args...))
		{
			// Successful line emission terminates before fallback can repeat it.
			::fast_io::fast_terminate();
		}
#ifdef __cpp_exceptions
	}
	catch (...)
	{
		// Convert any line-diagnostic exception to immediate termination.
		::fast_io::fast_terminate();
	}
#endif
#endif
	::fast_io::io::panic_details::fallback<true>(
		::std::forward<Args>(args)...);
}

// Allow debug print
#ifndef FAST_IO_DISABLE_DEBUG_PRINT
// With debugging. We output to POSIX fd or Win32 Handle directly instead of C's stdout.
/** @brief Emits debug arguments without a newline under guarded output lifetime. */
template <typename T, typename... Args>
inline constexpr void debug_print(T &&t, Args &&...args)
{
	constexpr bool device_and_type_ok{
		::fast_io::operations::defines::print_freestanding_okay_for_line<false, T, Args...>};
	if constexpr (device_and_type_ok)
	{
		// Emit to the explicit debug device under one success-only output guard.
		::fast_io::operations::basic_output_operation_guard<T &&> guard{t};
		decltype(auto) outref = guard.ref();
		::fast_io::operations::decay::
			print_freestanding_compiler_constant_pre_normalization<false>(
				outref, args...);
		guard.commit();
	}
	else
	{
		// Diagnose the explicit form or select the native debug-output fallback.
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		if constexpr (device_ok)
		{
			// The explicit debug device is valid, so diagnose its payload.
			if constexpr (::fast_io::details::has_raw_print_arg<Args...>)
			{
				// Produce the dedicated forbidden-raw-argument diagnostic.
				::fast_io::details::print_raw_static_assert<Args...>();
			}
			else
			{
				// Report the general explicit debug-print constraint failure.
				static_assert(device_and_type_ok,
							  "some types are not printable for debug_print on the provided output stream");
			}
		}
		else
		{
			// Treat all arguments as payload for the platform debug sink.
			constexpr bool type_ok{::fast_io::operations::defines::print_freestanding_params_okay<char, T, Args...>};
			if constexpr (type_ok)
			{
				// Attempt constant emission before entering the cold debug path.
				if (::fast_io::details::debug_print_try_default_compiler_constant<false>(
						t, args...))
				{
					// The constant arm already emitted the complete debug record.
					return;
				}
				fast_io::details::debug_print_after_source_pre_normalization<false>(
					t, args...);
			}
			else
			{
				// Diagnose invalid debug payload before observing the native sink.
				if constexpr (::fast_io::details::has_raw_print_arg<T, Args...>)
				{
					// Prefer the dedicated raw-argument diagnostic.
					::fast_io::details::print_raw_static_assert<T, Args...>();
				}
				else
				{
					// Report the native-debug printable constraint failure.
				// clang-format off
static_assert(type_ok, "some types are not printable for debug_print on native out");
				// clang-format on
				}
			}
		}
#else
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		// clang-format off
static_assert(device_ok, "freestanding environment must provide IO device for debug_print");
static_assert(device_and_type_ok, "some types are not printable for debug_print on native out");
		// clang-format on
#endif
	}
}

/** @brief Emits debug arguments plus a newline under guarded output lifetime. */
template <typename T, typename... Args>
inline constexpr void debug_println(T &&t, Args &&...args)
{
	constexpr bool device_and_type_ok{
		::fast_io::operations::defines::print_freestanding_okay_for_line<true, T, Args...>};
	if constexpr (device_and_type_ok)
	{
		// Emit the explicit debug line under one success-only output guard.
		::fast_io::operations::basic_output_operation_guard<T &&> guard{t};
		decltype(auto) outref = guard.ref();
		::fast_io::operations::decay::
			print_freestanding_compiler_constant_pre_normalization<true>(
				outref, args...);
		guard.commit();
	}
	else
	{
		// Diagnose the explicit form or select native debug-output fallback.
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		if constexpr (device_ok)
		{
			// The explicit debug device is valid, so diagnose its payload.
			if constexpr (::fast_io::details::has_raw_print_arg<Args...>)
			{
				// Produce the dedicated forbidden-raw-argument diagnostic.
				::fast_io::details::print_raw_static_assert<Args...>();
			}
			else
			{
				// Report the general explicit debug-line constraint failure.
				static_assert(device_and_type_ok,
							  "some types are not printable for debug_println on the provided output stream");
			}
		}
		else
		{
			// Treat all arguments as payload for the platform debug sink.
			constexpr bool type_ok{::fast_io::operations::defines::print_freestanding_params_okay<char, T, Args...>};
			if constexpr (type_ok)
			{
				// Attempt constant line emission before the cold debug path.
				if (::fast_io::details::debug_print_try_default_compiler_constant<true>(
						t, args...))
				{
					// The constant arm already emitted the complete debug line.
					return;
				}
				fast_io::details::debug_print_after_source_pre_normalization<true>(
					t, args...);
			}
			else
			{
				// Diagnose invalid debug payload before observing the native sink.
				if constexpr (::fast_io::details::has_raw_print_arg<T, Args...>)
				{
					// Prefer the dedicated raw-argument diagnostic.
					::fast_io::details::print_raw_static_assert<T, Args...>();
				}
				else
				{
					// Report the native-debug line-printable constraint failure.
				// clang-format off
static_assert(type_ok, "some types are not printable for debug_println on native out");
				// clang-format on
				}
			}
		}
#else
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		// clang-format off
static_assert(device_ok, "freestanding environment must provide IO device for debug_println");
static_assert(device_and_type_ok, "some types are not printable for debug_println on native out");
		// clang-format on
#endif
	}
}

template <typename... Args>
	requires(sizeof...(Args) != 0)
inline constexpr void debug_perr(Args &&...args)
{
	::fast_io::io::perr(::std::forward<Args>(args)...);
}

template <typename... Args>
	requires(sizeof...(Args) != 0)
inline constexpr void debug_perrln(Args &&...args)
{
	::fast_io::io::perrln(::std::forward<Args>(args)...);
}
#endif

#if defined(__clang__) && __clang_major__ == 23 && defined(__SSE4_1__) && \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) &&        \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
/*
Clang 23 assigns the native uint64 decimal scalar controller a cost above its
ordinary caller-inline threshold after the SSE parser is instantiated.  Keep
only the library-owned terminal view spelling at the public scalar boundary:
the named proxy still enters scan_single_impl, so cursor validation, report
mode, and parse errors have one implementation.  Signed targets (including
the historical terminal '-' rule), padded views, custom sources, status CPOs,
mutex streams, and multi-target packs remain on the general dispatcher.
*/
template <bool report = false, ::std::integral char_type,
		  ::fast_io::details::my_unsigned_integral T>
	requires(sizeof(char_type) == sizeof(char8_t) &&
			 ::fast_io::details::is_ascii<char_type> &&
			 sizeof(T) == sizeof(::std::uint_least64_t))
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline constexpr ::std::conditional_t<report, bool, void>
scan(::fast_io::basic_ibuffer_view<char_type> &in, T &value)
{
	auto inref{::fast_io::operations::input_stream_ref(in)};
	auto arg{::fast_io::io_scan_forward<char_type>(
		::fast_io::io_scan_alias(value))};
	if constexpr (report)
	{
		return ::fast_io::details::scan_single_impl<>(inref, arg);
	}
	else if (!::fast_io::details::scan_single_impl<>(inref, arg))
	{
		::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
	}
}
#endif

template <bool report = false, typename input, typename... Args>
inline constexpr ::std::conditional_t<report, bool, void> scan(input &&in, Args &&...args)
{
	// Normalization observes the named input as an lvalue, including when the
	// caller supplied a temporary whose full-expression lifetime covers scan.
	// Probe that exact expression: forwarding the owner here would change the
	// synchronous borrowing contract and could select an unrelated consuming CPO.
	constexpr bool device_error{::fast_io::operations::defines::has_input_or_io_stream_ref_define<
		::std::remove_reference_t<input> &>};
	if constexpr (device_error)
	{
		decltype(auto) inref = ::fast_io::operations::input_stream_ref(in);
		using char_type = typename ::std::remove_cvref_t<decltype(inref)>::input_char_type;
		if constexpr (report)
		{
			return ::fast_io::operations::decay::scan_freestanding_decay_borrowed_input(
				inref,
				::fast_io::io_scan_forward<char_type>(::fast_io::io_scan_alias(args))...);
		}
		else
		{
			if (!::fast_io::operations::decay::scan_freestanding_decay_borrowed_input(
					inref,
					::fast_io::io_scan_forward<char_type>(::fast_io::io_scan_alias(args))...))
			{
				::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
			}
		}
	}
	else
	{
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES)) &&                                     \
	__has_include(<stdio.h>)
		return ::fast_io::details::scan_after_io_scan_forward<report>(
			::fast_io::io_scan_forward<char>(::fast_io::io_scan_alias(in)),
			::fast_io::io_scan_forward<char>(::fast_io::io_scan_alias(args))...);
#else
		// clang-format off
static_assert(device_error, "freestanding environment must provide IO device");
		// clang-format on
#endif
	}
}

} // namespace io

namespace iomnp
{
using namespace ::fast_io::io;
using namespace ::fast_io::mnp;
} // namespace iomnp

} // namespace fast_io
