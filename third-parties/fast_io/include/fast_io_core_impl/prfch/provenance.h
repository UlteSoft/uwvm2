#pragma once

namespace fast_io
{

/// @brief Proves that an allocator returns ordinary processor-cacheable storage.
/// @details Allocation APIs prove ownership and alignment, not the memory domain. This is therefore an exact-true ADL
///          opt-in rather than a structural allocator test: custom allocators may legally serve persistent mappings,
///          device apertures, shared accelerator memory, or other storage for which a CPU prefetch is inappropriate.
///          The marker is allocator-level evidence only; a consumer must still prove the concrete allocation's live
///          bounds and choose the read or write direction required by its operation.
template <typename T>
concept prfch_cacheable_allocator_provenance = requires {
	{
		prfch_cacheable_allocator_provenance_define(
			::fast_io::io_type_t<::std::remove_cvref_t<T>>{})
	} -> ::std::same_as<::std::true_type>;
};

namespace details
{

/// @brief Records whether this build's native global alias selected a library-known cacheable backend.
/// @details `native_global_allocator` is only a policy alias. User-selected custom allocation and the freestanding
///          fallback both use `custom_global_allocator`, whose memory domain is intentionally unknown. The positive
///          cases mirror allocation/impl.h: mimalloc, kernel kmalloc, or a hosted malloc/Windows-heap implementation.
inline constexpr bool prfch_native_global_allocator_cacheable{
#if !defined(FAST_IO_USE_CUSTOM_GLOBAL_ALLOCATOR) &&                                                 \
	(((defined(FAST_IO_USE_MIMALLOC) && (!defined(_MSC_VER) || defined(__clang__)))) ||              \
	 ((defined(__linux__) && defined(__KERNEL__)) || defined(FAST_IO_USE_LINUX_KERNEL_ALLOCATOR)) || \
	 ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) &&                \
	   !defined(_LIBCPP_FREESTANDING)) ||                                                            \
	  defined(FAST_IO_ENABLE_HOSTED_FEATURES)))
	true
#else
	false
#endif
};

/// @brief The native thread-local alias is known only when it delegates to a known native global backend.
/// @details `FAST_IO_USE_CUSTOM_THREAD_LOCAL_ALLOCATOR` replaces that delegation and must supply its own exact ADL
///          opt-in if its allocation domain is cacheable.
inline constexpr bool prfch_native_thread_local_allocator_cacheable{
#if !defined(FAST_IO_USE_CUSTOM_THREAD_LOCAL_ALLOCATOR)
	prfch_native_global_allocator_cacheable
#else
	false
#endif
};

} // namespace details

/// @brief Library defaults for native aliases whose selected backend is known in this build.
/// @details The constrained type equality is intentional. Derivation or protocol compatibility cannot prove an
///          allocator's memory domain, while a custom allocator can provide its own exact-true ADL marker explicitly.
template <typename allocator_type>
	requires((::std::same_as<allocator_type, native_global_allocator> &&
			  ::fast_io::details::prfch_native_global_allocator_cacheable) ||
			 (::std::same_as<allocator_type, native_thread_local_allocator> &&
			  ::fast_io::details::prfch_native_thread_local_allocator_cacheable))
inline constexpr ::std::true_type prfch_cacheable_allocator_provenance_define(
	io_type_t<allocator_type>) noexcept
{
	return {};
}

/// @brief Introduces read-prefetch provenance only for the explicit proof-carrying scatter.
/// @details The raw scatter remains unmarked because its pointer can name MMIO or non-cacheable mappings. This marker
///          is intentionally direction-specific: an ordinary readable source is not necessarily a safe write-prefetch
///          destination.
template <typename T>
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	io_type_t<basic_prfch_cacheable_io_scatter_t<T>>) noexcept
{
	return {};
}

/// @brief Proves that memory ranges advertised by a type are ordinary cacheable read sources.
/// @details A prefetch instruction is only a hint to the processor; that does not make an arbitrary C++ address a
///          suitable hint target. In particular, a raw pointer or a scatter descriptor may name volatile storage,
///          memory-mapped device registers, a transient mapping, or storage whose lifetime ends before the hint is
///          consumed. This concept is therefore an explicit ADL opt-in rather than a structural inference. The marker
///          author promises that every range which the marked type exposes to a prefetch-enabled operation is backed by
///          ordinary cacheable memory and remains readable for that complete operation.
///
///          The promise is intentionally necessary but not sufficient. A caller must independently prove the concrete
///          range's lifetime and bounds, reject an empty/null descriptor, and form the hinted address within that live
///          range. A retained print scatter must additionally satisfy the existing borrowed/repeatable provenance
///          contract; this marker does not upgrade a transient descriptor into a retained one. No pointer, span,
///          stream, or `basic_io_scatter_t` receives a library default, because none of those shapes proves where its
///          storage came from.
///
///          Cacheability is not a side-channel proof. A hint can change cache and translation state even though program
///          correctness cannot depend on it. A site policy must therefore avoid introducing a new secret-dependent or
///          attacker-selected address observation; this provenance marker alone never authorizes such a policy.
/// @fn      prfch_cacheable_read_provenance_define
/// @return  std::true_type
template <typename T>
concept prfch_cacheable_read_provenance = requires {
	{
		prfch_cacheable_read_provenance_define(
			::fast_io::io_type_t<::std::remove_cvref_t<T>>{})
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Proves that memory ranges advertised by a type are ordinary cacheable write destinations.
/// @details This is a separate opt-in from readable provenance. A source can be immutable, while a writable mapping
///          may carry device or persistence semantics for which a write-prefetch is not an observationally neutral
///          optimization. The marker author promises that every destination range offered to a prefetch-enabled
///          operation is writable ordinary cacheable memory, that its backing allocation remains alive until the
///          operation completes, and that a processor write-allocation hint has no externally visible device effect.
///
///          Cursor availability, spare capacity, and this marker prove different facts. Print/concat code must still
///          establish that the selected address lies in the currently writable put area; scan code must establish the
///          equivalent fact for a destination field or owned staging buffer. The library deliberately provides no
///          default for an output observer merely because it exposes `obuffer_curr` and `obuffer_end`.
/// @fn      prfch_cacheable_write_provenance_define
/// @return  std::true_type
template <typename T>
concept prfch_cacheable_write_provenance = requires {
	{
		prfch_cacheable_write_provenance_define(
			::fast_io::io_type_t<::std::remove_cvref_t<T>>{})
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Convenience refinement for an owner which independently supplies both provenance promises.
/// @details The conjunction is deliberate: a single read/write-shaped marker would make accidental direction
///          widening easy and would prevent read-only retained sources from expressing the minimum contract.
template <typename T>
concept prfch_cacheable_read_write_provenance =
	prfch_cacheable_read_provenance<T> && prfch_cacheable_write_provenance<T>;

/// @brief Propagates an established read proof through the normalized `parameter` transport.
/// @details Entry decay uses `parameter<T>` to preserve an identity-sensitive lvalue while reducing downstream type
///          variation. The wrapper neither changes the referent nor manufactures a range, so dropping provenance here
///          would make equivalent by-value and by-reference entry paths select different prefetch policies.
template <typename T>
	requires prfch_cacheable_read_provenance<::std::remove_cvref_t<T>>
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(io_type_t<parameter<T>>) noexcept
{
	return {};
}

namespace details
{

/// @brief Identifies semantic children which expose no external read range at all.
/// @details This is a composition aid, not a prefetch provenance marker. In particular `io_null_t` must not become a
///          hint target merely because it contributes no bytes. `parameter` propagation is recursive so semantic
///          storage does not lose the vacuous proof after entry normalization.
template <typename T>
struct prfch_no_external_read_range : ::std::false_type
{};

template <>
struct prfch_no_external_read_range<io_null_t> : ::std::true_type
{};

template <typename T>
struct prfch_no_external_read_range<parameter<T>>
	: prfch_no_external_read_range<::std::remove_cvref_t<T>>
{};

} // namespace details

/// @brief Admits a semantic child only when every possible external range is proved cacheable.
/// @details Composite manipulators use this refinement for alternatives or element lists. A no-output child satisfies
///          it vacuously, while a raw pointer-shaped descriptor does not.
template <typename T>
concept prfch_cacheable_read_or_no_external_range =
	prfch_cacheable_read_provenance<T> ||
	::fast_io::details::prfch_no_external_read_range<::std::remove_cvref_t<T>>::value;

} // namespace fast_io
