#pragma once

namespace fast_io
{

/// @brief Describes the access expected to follow a prefetch hint.
/// @details `read` and `write` are intent, not guarantees that the backend exposes distinct instructions. The
///          `instruction` mode is emitted only by a target-specific instruction-prefetch lowering; it never falls back
///          to a data-prefetch builtin merely because that builtin accepts read/write locality arguments. In
///          particular, a generic write request may degrade to read-warming on a target without a distinct write hint;
///          no separate write-distinct capability is claimed from GCC's `__PRFCHW__` macro.
enum class prfch_mode
{
	read = 0,
	write = 1,
	instruction = 2
};

/// @brief Portable locality vocabulary for prefetch hints.
/// @details These names describe the intended closest useful cache level. They are not a promise that a target exposes
///          that exact hierarchy: GCC/Clang's generic builtin accepts only a locality value, and hardware is permitted
///          to ignore or reinterpret every hint.
enum class prfch_level
{
	nta = 0,
	L3 = 1,
	L2 = 2,
	L1 = 3
};

/// @brief Expresses whether the hinted line should be retained or treated as streaming data.
/// @details AArch64 preserves this distinction in its PRFM operation. GCC/Clang's generic prefetch builtin and MSVC's
///          x86 intrinsic expose only their own locality vocabulary, so this intent can be folded into `prfch_level` or
///          ignored by the selected lowering.
enum class prfch_retention
{
	keep = 0,
	strm = 1
};

namespace details
{

template <prfch_level level>
inline constexpr unsigned aarch64_prfch_cache_level{
	level == prfch_level::nta ? 0u : 3u - static_cast<unsigned>(level)};

template <prfch_level level, prfch_retention retention>
inline constexpr unsigned aarch64_prfch_retention{
	level == prfch_level::nta ? 1u : static_cast<unsigned>(retention)};

/// @brief Encodes an AArch64 PRFM operation without depending on an assembler spelling.
/// @details The architectural immediate uses bits [4:3] for PLD/PLI/PST, bits [2:1] for L1/L2/L3, and bit 0 for
///          KEEP/STRM. `nta` has no independent architectural cache level, so the portable NTA request maps to the
///          conventional L1 streaming hint. Keeping this proof in one constexpr expression prevents the Clang ACLE
///          and genuine-MSVC paths from silently acquiring different semantics.
template <prfch_mode mode, prfch_level level, prfch_retention retention>
inline constexpr unsigned char aarch64_prfch_operation = []() constexpr noexcept {
	constexpr unsigned access_kind{mode == prfch_mode::read ? 0u : mode == prfch_mode::instruction ? 1u
																								   : 2u};
	constexpr unsigned cache_level{::fast_io::details::aarch64_prfch_cache_level<level>};
	constexpr unsigned retention_kind{::fast_io::details::aarch64_prfch_retention<level, retention>};
	return static_cast<unsigned char>((access_kind << 3u) | (cache_level << 1u) | retention_kind);
}();

template <typename T>
concept prfch_function_pointer =
	::std::is_pointer_v<T> && ::std::is_function_v<::std::remove_pointer_t<T>>;

} // namespace details

/// @brief Explicit caller assertion that a function symbol has non-preemptible local binding.
/// @details This token is intentionally stronger than a function-pointer non-type template argument. Standard C++ can
///          verify that `function_address` points to a function, but it cannot inspect the final ELF/COFF binding or
///          linker interposition rules. Instantiating this type promises that every supported link mode resolves the
///          symbol locally (for example, an internal-linkage function or a correctly hidden symbol). Passing a
///          default-visible or undefined preemptible symbol violates the precondition and may make genuine GCC emit a
///          diagnostic which becomes a hard error under `-Werror`. The library does not infer or manufacture this
///          assertion from an ordinary function pointer.
template <auto function_address>
	requires(::fast_io::details::prfch_function_pointer<decltype(function_address)> &&
			 function_address != nullptr)
struct prfch_local_function_t
{
	inline static constexpr auto address{function_address};
};

namespace details
{

template <typename>
inline constexpr bool is_local_prfch_function_binding{false};

template <auto function_address>
inline constexpr bool is_local_prfch_function_binding<
	::fast_io::prfch_local_function_t<function_address>>{true};

} // namespace details

/// @brief Recognizes only the library token carrying the caller's local-binding assertion.
/// @details A structurally similar user type and a raw function-pointer type intentionally do not satisfy the concept;
///          spelling `prfch_local_function_t<&function>` at the call site makes the non-language precondition visible.
template <typename T>
concept local_prfch_function_binding =
	::fast_io::details::is_local_prfch_function_binding<::std::remove_cvref_t<T>>;

/// @brief Emits the closest available non-faulting prefetch hint, or a compile-time no-op on unsupported targets.
/// @details The function itself deliberately performs no run-time feature detection. Data prefetch uses the compiler's
///          generic builtin where available, which GCC documents as a no-op when the target has no lowering. Genuine
///          MSVC uses ISA-specific intrinsics and conservatively degrades write intent to the corresponding read hint
///          on x86 rather than assuming PREFETCHW support. AArch64 supports a general-address instruction hint. Clang
///          x86 also preserves its pointer-shaped PREFETCHI builtin, but the architecture acts only on a RIP-relative
///          operand; a dynamic/register form is ignored. Genuine GCC's stricter builtin is therefore omitted from this
///          generic entry and exposed through `prfch_instruction_local` instead. The address expression must still be
///          valid C++ even though the generated machine hint is non-faulting. Higher-level bounded helpers own that
///          object-range proof.
template <prfch_mode mode = prfch_mode::write, prfch_level level = prfch_level::nta,
		  prfch_retention retention = prfch_retention::keep>
	requires(static_cast<unsigned>(mode) < 3u && static_cast<unsigned>(level) < 4u &&
			 static_cast<unsigned>(retention) < 2u)
FAST_IO_GNU_ALWAYS_INLINE_ARTIFICIAL inline constexpr void prfch(void const *address) noexcept
{
	if (::std::is_constant_evaluated())
	{
		return;
	}
	if constexpr (mode == prfch_mode::instruction)
	{
#if (defined(__aarch64__) || defined(__arm64__) || defined(__arm64) || defined(__arm64ec__) || \
	 defined(_M_ARM64) || defined(_M_ARM64EC)) &&                                              \
	FAST_IO_HAS_BUILTIN(__builtin_arm_prefetch)
		if constexpr (level == prfch_level::nta)
		{
			__builtin_arm_prefetch(address, 0, 0, 1, 0);
		}
		else
		{
			__builtin_arm_prefetch(address, 0,
								   static_cast<int>(::fast_io::details::aarch64_prfch_cache_level<level>),
								   static_cast<int>(::fast_io::details::aarch64_prfch_retention<level, retention>), 0);
		}
#elif (defined(__aarch64__) || defined(__arm64__) || defined(__arm64)) && \
	FAST_IO_HAS_BUILTIN(__builtin_aarch64_plix)
		__builtin_aarch64_plix(::fast_io::details::aarch64_prfch_cache_level<level>,
							   ::fast_io::details::aarch64_prfch_retention<level, retention>, address);
#elif defined(__aarch64__) && defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 16
		__plix(::fast_io::details::aarch64_prfch_cache_level<level>,
			   ::fast_io::details::aarch64_prfch_retention<level, retention>, address);
#elif defined(__arm__) && FAST_IO_HAS_BUILTIN(__builtin_arm_prefetch)
		// The ARM32 ACLE builtin has the three-argument form and exposes PLI without cache-level/retention controls.
		__builtin_arm_prefetch(address, 0, 0);
#elif defined(_MSC_VER) && !defined(__clang__) && (defined(_M_ARM64) || defined(_M_ARM64EC))
		__prefetch2(address, details::aarch64_prfch_operation<mode, level, retention>);
#elif defined(__x86_64__) && defined(__PREFETCHI__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_prefetchi) && \
	defined(__clang__) && !defined(__INTEL_LLVM_COMPILER) &&                                            \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
		// Clang deliberately keeps the intrinsic's pointer syntax. A non-RIP-relative lowering is architecturally
		// ignored; callers requiring an effective hint should use the explicit local-binding token overload.
		constexpr int actual_level{static_cast<int>(level) < 2 ? 2 : static_cast<int>(level)};
		__builtin_ia32_prefetchi(address, actual_level);
#else
		(void)address;
#endif
	}
	else
	{
#if (defined(__aarch64__) || defined(__arm64__) || defined(__arm64) || defined(__arm64ec__) || \
	 defined(_M_ARM64) || defined(_M_ARM64EC)) &&                                              \
	FAST_IO_HAS_BUILTIN(__builtin_arm_prefetch)
		if constexpr (level == prfch_level::nta)
		{
			__builtin_arm_prefetch(address, static_cast<int>(mode), 0, 1, 1);
		}
		else
		{
			__builtin_arm_prefetch(address, static_cast<int>(mode),
								   static_cast<int>(::fast_io::details::aarch64_prfch_cache_level<level>),
								   static_cast<int>(::fast_io::details::aarch64_prfch_retention<level, retention>), 1);
		}
#elif (defined(__aarch64__) || defined(__arm64__) || defined(__arm64)) && \
	FAST_IO_HAS_BUILTIN(__builtin_aarch64_pldx)
		__builtin_aarch64_pldx(mode == prfch_mode::read ? 0u : 1u,
							   ::fast_io::details::aarch64_prfch_cache_level<level>,
							   ::fast_io::details::aarch64_prfch_retention<level, retention>, address);
#elif defined(__aarch64__) && defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 16
		__pldx(mode == prfch_mode::read ? 0u : 1u,
			   ::fast_io::details::aarch64_prfch_cache_level<level>,
			   ::fast_io::details::aarch64_prfch_retention<level, retention>, address);
#elif FAST_IO_HAS_BUILTIN(__builtin_prefetch)
		__builtin_prefetch(address, static_cast<int>(mode), static_cast<int>(level));
#elif defined(_MSC_VER) && !defined(__clang__) && (defined(_M_ARM64) || defined(_M_ARM64EC))
		__prefetch2(address, details::aarch64_prfch_operation<mode, level, retention>);
#elif defined(_MSC_VER) && !defined(__clang__) && defined(_M_ARM)
		__prefetch(address);
#elif defined(_MSC_VER) && !defined(__clang__) &&                                         \
	(defined(_M_X64) || defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
		// Name every cache level explicitly. The MSVC constants and GNU locality values are both numbered 0/1/2/3,
		// but T0/T1/T2 describe proximity in the reverse textual order from the portable L1/L2/L3 vocabulary.
		if constexpr (level == prfch_level::nta)
		{
			_mm_prefetch(reinterpret_cast<char const *>(address), _MM_HINT_NTA);
		}
		else if constexpr (level == prfch_level::L3)
		{
			_mm_prefetch(reinterpret_cast<char const *>(address), _MM_HINT_T2);
		}
		else if constexpr (level == prfch_level::L2)
		{
			_mm_prefetch(reinterpret_cast<char const *>(address), _MM_HINT_T1);
		}
		else
		{
			_mm_prefetch(reinterpret_cast<char const *>(address), _MM_HINT_T0);
		}
#else
		(void)address;
#endif
	}
}

/// @brief Applies the available direct instruction-prefetch lowering to a statically named function.
/// @details AArch64 and ARM forward the function address to their dynamic PLI lowering. Clang x86 preserves its
///          pointer-shaped builtin here; an internal or hidden target can become RIP-relative, while a preemptible
///          target may lower to an architecturally ignored register form. Genuine GCC keeps this convenience a no-op
///          because a function-pointer NTTP alone does not satisfy its operand constraint. Callers which own the
///          stronger binding proof should use `prfch_instruction_local` on either compiler.
template <auto function_address, prfch_level level = prfch_level::L1,
		  prfch_retention retention = prfch_retention::keep>
	requires(::fast_io::details::prfch_function_pointer<decltype(function_address)> &&
			 function_address != nullptr && static_cast<unsigned>(level) < 4u &&
			 static_cast<unsigned>(retention) < 2u)
FAST_IO_GNU_ALWAYS_INLINE_ARTIFICIAL inline constexpr void prfch_instruction_direct() noexcept
{
	if (::std::is_constant_evaluated())
	{
		return;
	}
#if (defined(__aarch64__) || defined(__arm64__) || defined(__arm64) || defined(__arm64ec__) ||         \
	 defined(_M_ARM64) || defined(_M_ARM64EC) || defined(__arm__)) ||                                  \
	(defined(__x86_64__) && defined(__PREFETCHI__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_prefetchi) && \
	 defined(__clang__) && !defined(__INTEL_LLVM_COMPILER) &&                                          \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC)))
	::fast_io::prfch<prfch_mode::instruction, level, retention>(
		reinterpret_cast<void const *>(function_address));
#endif
}

/// @brief Prefetches a caller-asserted locally-bound function with Clang or genuine GCC's x86 PREFETCHI builtin.
/// @details The binding token is a precondition, not a compiler-verified proof. In PIC code an ordinary external or
///          default-visible function can require a GOT load and therefore cannot form GCC's required RIP-relative
///          operand; lying in the token can produce a diagnostic (and a hard error under strict warning policies).
///          Clang does not enforce the same failure mode: a false assertion can become a register-indirect PREFETCHI,
///          which the architecture ignores. Internal linkage or a link-mode-independent hidden/local binding is thus
///          required on both compilers, although only GCC diagnoses common violations. All unsupported compiler/ISA
///          combinations are compile-time no-ops, preserving source portability without weakening the assertion.
template <local_prfch_function_binding binding_type, prfch_level level = prfch_level::L1,
		  prfch_retention retention = prfch_retention::keep>
	requires(static_cast<unsigned>(level) < 4u && static_cast<unsigned>(retention) < 2u)
FAST_IO_GNU_ALWAYS_INLINE_ARTIFICIAL inline constexpr void prfch_instruction_local() noexcept
{
	if (::std::is_constant_evaluated())
	{
		return;
	}
#if (defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)) && defined(__PREFETCHI__) && \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_prefetchi) &&                                           \
	((defined(__clang__) && !defined(__INTEL_LLVM_COMPILER)) ||                                \
	 (defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) &&                \
	  !defined(__INTEL_LLVM_COMPILER))) &&                                                     \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	constexpr int actual_level{static_cast<int>(level) < 2 ? 2 : static_cast<int>(level)};
	__builtin_ia32_prefetchi(reinterpret_cast<void const *>(binding_type::address), actual_level);
#endif
}

} // namespace fast_io
