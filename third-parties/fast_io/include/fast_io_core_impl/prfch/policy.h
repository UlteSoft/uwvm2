#pragma once

#include "platform.h"
#include "provenance.h"

namespace fast_io
{

namespace details
{

/*
Policy-maintenance rule for a newly qualified architecture
----------------------------------------------------------
`conservative_*_prfch_platform_impl` is the broad experimental permission layer; every later predicate in this file is
a site-specific profitability layer. Extend the read and write predicates independently, even when the ISA encodes
both hints symmetrically. A positive broad result must never be copied into a site predicate without retained
measurements of that exact consuming memory operation and a public-entry code-generation check. When a site is
admitted, encode its measured trip count, payload, lookahead, cache level, retention, provenance, lifetime, and
in-range-address premises next to the site predicate. Keep unsupported directions as explicit false predicates so
later work cannot mistake an absent experiment for an implementation omission. Follow the complete qualification
protocol beside `prfch_tune` in `platform.h` and retain all evidence in `benchmark/0022.prfch/`.
*/

/// @brief Forms the initial broad-core allow-list for manually prefetched reads.
/// @details This is a permission envelope, not a command to emit a hint. The selected tune families describe modern
///          out-of-order cores for which the backend has a stable data-prefetch lowering and for which one conservative
///          policy can be evaluated across several processors. `generic` is rejected because it supplies no evidence
///          about the actual core. Atom, legacy AMD, and unclassified tune families remain disabled until an
///          independently retained benchmark demonstrates a broad win; ISA membership alone is not such evidence.
///
///          A hot-path strategy must add a measured size/distance threshold and a concrete in-range lifetime proof.
///          Keeping those site-dependent facts out of this classifier prevents a successful scatter-copy experiment
///          from silently authorizing the same distance in a contiguous parser whose hardware prefetcher already wins.
template <typename platform_type>
inline consteval bool conservative_read_prfch_platform_impl() noexcept
{
	if constexpr (!::fast_io::data_prfch_platform<platform_type>)
	{
		return false;
	}
	else
	{
		constexpr auto isa{static_cast<::fast_io::prfch_isa>(platform_type::isa)};
		constexpr auto tune{static_cast<::fast_io::prfch_tune>(platform_type::tune)};
		if constexpr (isa == ::fast_io::prfch_isa::x86)
		{
			return tune == ::fast_io::prfch_tune::x86_intel_core ||
				   tune == ::fast_io::prfch_tune::x86_intel_hybrid ||
				   tune == ::fast_io::prfch_tune::x86_amd_zen;
		}
		else if constexpr (isa == ::fast_io::prfch_isa::aarch64)
		{
			return tune == ::fast_io::prfch_tune::arm_apple ||
				   tune == ::fast_io::prfch_tune::arm_application ||
				   tune == ::fast_io::prfch_tune::arm_server;
		}
		else
		{
			return false;
		}
	}
}

/// @brief Forms the independently extensible broad-core allow-list for manually prefetched writes.
/// @details The initial read and write sets are intentionally equal but are implemented separately. Read and write
///          hints have different lowering and cache-allocation consequences on several targets, so evidence which
///          extends one direction must not widen the other by construction. As with the read policy, a true result only
///          permits a site-specific, benchmarked strategy to be considered.
template <typename platform_type>
inline consteval bool conservative_write_prfch_platform_impl() noexcept
{
	if constexpr (!::fast_io::data_prfch_platform<platform_type>)
	{
		return false;
	}
	else
	{
		constexpr auto isa{static_cast<::fast_io::prfch_isa>(platform_type::isa)};
		constexpr auto tune{static_cast<::fast_io::prfch_tune>(platform_type::tune)};
		if constexpr (isa == ::fast_io::prfch_isa::x86)
		{
			return tune == ::fast_io::prfch_tune::x86_intel_core ||
				   tune == ::fast_io::prfch_tune::x86_intel_hybrid ||
				   tune == ::fast_io::prfch_tune::x86_amd_zen;
		}
		else if constexpr (isa == ::fast_io::prfch_isa::aarch64)
		{
			return tune == ::fast_io::prfch_tune::arm_apple ||
				   tune == ::fast_io::prfch_tune::arm_application ||
				   tune == ::fast_io::prfch_tune::arm_server;
		}
		else
		{
			return false;
		}
	}
}

} // namespace details

/// @brief Identifies a compiler target/tune pair eligible for a conservative read-prefetch experiment.
/// @details `true` proves only platform eligibility. It deliberately says nothing about a particular object's
///          cacheability, range bounds, access order, trip count, hint distance, or measured profitability.
template <typename platform_type>
concept conservative_read_prfch_platform =
	::fast_io::prfch_platform<platform_type> &&
	::fast_io::details::conservative_read_prfch_platform_impl<platform_type>();

/// @brief Write-direction counterpart of `conservative_read_prfch_platform`.
template <typename platform_type>
concept conservative_write_prfch_platform =
	::fast_io::prfch_platform<platform_type> &&
	::fast_io::details::conservative_write_prfch_platform_impl<platform_type>();

/// @brief Combines the platform allow-list with an explicit cacheable-read provenance promise.
/// @details This is the reusable semantic gate for print, concat, scan, and semantic-manipulator policies. It remains a
///          necessary-only proof: the consuming loop must additionally validate a nonempty live range and apply its own
///          retained benchmark threshold before calling `prfch`. Separating platform and provenance concepts makes a
///          failed requirement diagnostically visible and prevents an unmarked user stream from inheriting permission
///          through an unrelated ISA match.
template <typename platform_type, typename provenance_type>
concept conservative_read_prfch_strategy =
	::fast_io::conservative_read_prfch_platform<platform_type> &&
	::fast_io::prfch_cacheable_read_provenance<provenance_type>;

/// @brief Combines the platform allow-list with an explicit cacheable-write provenance promise.
/// @details Output buffering or writable cursor syntax alone never satisfies this concept. An internal owned-buffer
///          type may opt in with `prfch_cacheable_write_provenance_define`; a retained-scatter operation normally uses
///          the read counterpart on its producer and this write counterpart only when the destination owner supplies
///          an independent ordinary-memory promise.
template <typename platform_type, typename provenance_type>
concept conservative_write_prfch_strategy =
	::fast_io::conservative_write_prfch_platform<platform_type> &&
	::fast_io::prfch_cacheable_write_provenance<provenance_type>;

/// @brief Complete bounded envelope for a one-descriptor-ahead scatter-chain prefetch site.
/// @details Minimum and maximum bounds are both semantic. A maximum is not documentation-only: the sizing traversal
///          must reject a chain outside the measured envelope before the copy traversal emits a hint. A target may
///          further restrict the envelope to retained discrete values; callers must use the policy eligibility
///          predicates below rather than interpreting this aggregate as an automatically continuous rectangle.
///          Keeping cache level and retention in the same value prevents a platform gate from accidentally reusing
///          another target's instruction intent.
struct scatter_chain_prfch_policy
{
	::std::size_t minimum_descriptor_count{};
	::std::size_t maximum_descriptor_count{};
	::std::size_t minimum_payload_bytes{};
	::std::size_t maximum_payload_bytes{};
	::fast_io::prfch_level level{};
	::fast_io::prfch_retention retention{};
};

namespace details
{

template <typename platform_type>
inline consteval ::fast_io::scatter_chain_prfch_policy
concat_scatter_chain_read_prfch_policy_impl() noexcept
{
	constexpr auto isa{static_cast<::fast_io::prfch_isa>(platform_type::isa)};
	constexpr auto tune{static_cast<::fast_io::prfch_tune>(platform_type::tune)};
	if constexpr (isa == ::fast_io::prfch_isa::aarch64 &&
				  tune == ::fast_io::prfch_tune::arm_apple)
	{
		// This is the bounded envelope of the discrete P/E-core grid retained on M4, not the wider P-core-only
		// rectangle. The eligibility predicates below reject unmeasured intermediate values.
		return {512u, 1024u, 1024u, 2048u, ::fast_io::prfch_level::L1,
				::fast_io::prfch_retention::keep};
	}
	else
	{
		return {32u, SIZE_MAX, 4u * 1024u, SIZE_MAX, ::fast_io::prfch_level::L1,
				::fast_io::prfch_retention::keep};
	}
}

/// @brief Selects the measured platform envelope for concat's materialized scatter-chain read site.
/// @details This predicate is intentionally narrower than `conservative_read_prfch_platform`. Three paired Linux
///          P-core seeds measured roughly 1--2.4 percent gains for cold discontinuous 4--16 KiB payloads, while the hot
///          controls stayed within about 0.2 percent. A later three-process E-core confirmation on the same hybrid CPU
///          retained neutral-or-better results for every 4/16-KiB hot/cold case. The earlier screen also found 17--31
///          percent hot regressions at 256/512 bytes. Those results admit only the measured hybrid tune family and the
///          all-large threshold for `x86_intel_hybrid`; `x86_intel_core`, AMD Zen, and generic AArch64 remain disabled.
///          Apple uses a separate discrete grid rather than inheriting that rule. The M4 P-core screen retained a
///          wider L1/keep read window, but corrected dense interpolation rejected a continuous rectangle. The common
///          P/E-core result contains descriptor counts 512, 768, and 1024, each with a uniform 1024- or 2048-byte
///          payload. The Apple family admission requested for `__APPLE__` plus AArch64 encodes only those six shapes.
///          M1--M5 still share one compile-time tune and one llvm-mca model, so this policy must not be described or
///          widened as cross-product runtime proof.
template <typename platform_type>
inline consteval bool concat_scatter_chain_read_prfch_platform_impl() noexcept
{
	if constexpr (!::fast_io::data_prfch_platform<platform_type>)
	{
		return false;
	}
	else
	{
		constexpr auto isa{static_cast<::fast_io::prfch_isa>(platform_type::isa)};
		constexpr auto tune{static_cast<::fast_io::prfch_tune>(platform_type::tune)};
		return (isa == ::fast_io::prfch_isa::x86 &&
				tune == ::fast_io::prfch_tune::x86_intel_hybrid) ||
			   (isa == ::fast_io::prfch_isa::aarch64 &&
				tune == ::fast_io::prfch_tune::arm_apple);
	}
}

/// @brief Keeps concat scatter-chain write prefetch disabled pending independent evidence.
/// @details A write hint can allocate a cache line or lower differently from a read hint. Mirroring a profitable
///          producer-read policy into the destination direction would therefore be an unsupported cost inference.
///          The initial M4 cross-core screen retained a single PSTL2STRM candidate at 1024 destinations by 8192 bytes,
///          but a later threshold-matched E-core process produced a 1.002815 hot median-time ratio. The all-process
///          rule therefore rejects even that exact point. Keeping a separately named false predicate makes a future
///          write experiment local to this site and direction without publishing a failed candidate as policy.
template <typename platform_type>
inline consteval bool concat_scatter_chain_write_prfch_platform_impl() noexcept
{
	return false;
}

} // namespace details

/// @brief Platform-specific bounded policy selected by concat's retained scatter-chain read site.
template <::fast_io::prfch_platform platform_type>
inline constexpr ::fast_io::scatter_chain_prfch_policy concat_scatter_chain_read_prfch_policy_for{
	::fast_io::details::concat_scatter_chain_read_prfch_policy_impl<platform_type>()};

/// @brief Tests the complete descriptor-count domain retained for one platform's concat read policy.
/// @details Apple M4 interpolation found regressions at intermediate points, so its envelope is deliberately a
///          discrete set rather than a range. The x86 hybrid policy remains its measured minimum-only interval.
template <::fast_io::prfch_platform platform_type>
inline constexpr bool concat_scatter_chain_read_prfch_descriptor_count_eligible(
	::std::size_t descriptor_count) noexcept
{
	auto constexpr policy{
		::fast_io::concat_scatter_chain_read_prfch_policy_for<platform_type>};
	if constexpr (static_cast<::fast_io::prfch_isa>(platform_type::isa) ==
					  ::fast_io::prfch_isa::aarch64 &&
				  static_cast<::fast_io::prfch_tune>(platform_type::tune) ==
					  ::fast_io::prfch_tune::arm_apple)
	{
		return descriptor_count == 512u || descriptor_count == 768u ||
			   descriptor_count == 1024u;
	}
	else
	{
		return policy.minimum_descriptor_count <= descriptor_count &&
			   descriptor_count <= policy.maximum_descriptor_count;
	}
}

/// @brief Tests the complete per-descriptor byte domain retained for one platform's concat read policy.
/// @details Apple admits only the two payload sizes measured on both M4 core classes. The sizing traversal separately
///          proves that all nonempty Apple payloads have the same size; mixed 1024/2048-byte chains were not measured.
template <::fast_io::prfch_platform platform_type>
inline constexpr bool concat_scatter_chain_read_prfch_payload_bytes_eligible(
	::std::size_t payload_bytes) noexcept
{
	auto constexpr policy{
		::fast_io::concat_scatter_chain_read_prfch_policy_for<platform_type>};
	if constexpr (static_cast<::fast_io::prfch_isa>(platform_type::isa) ==
					  ::fast_io::prfch_isa::aarch64 &&
				  static_cast<::fast_io::prfch_tune>(platform_type::tune) ==
					  ::fast_io::prfch_tune::arm_apple)
	{
		return payload_bytes == 1024u || payload_bytes == 2048u;
	}
	else
	{
		return policy.minimum_payload_bytes <= payload_bytes &&
			   payload_bytes <= policy.maximum_payload_bytes;
	}
}

/// @brief Whether the retained platform evidence requires one uniform nonempty payload size across the chain.
template <::fast_io::prfch_platform platform_type>
inline constexpr bool concat_scatter_chain_read_prfch_requires_uniform_payload{
	static_cast<::fast_io::prfch_isa>(platform_type::isa) ==
		::fast_io::prfch_isa::aarch64 &&
	static_cast<::fast_io::prfch_tune>(platform_type::tune) ==
		::fast_io::prfch_tune::arm_apple};

/// @brief Historical x86 minimum retained descriptor capacity admitted by concat's read-prefetch site.
/// @details Smaller chains do not provide enough repeated irregular work to amortize policy and lookahead overhead in
///          the retained Linux experiment. Reserve-scatters plans must still validate their smaller actual prefix at
///          run time because a customization may legally return fewer descriptors than its static capacity; zero-
///          length descriptors do not count toward the required 32 live payloads.
inline constexpr ::std::size_t concat_scatter_chain_read_prfch_minimum_descriptor_count{32u};

/// @brief Historical x86 minimum byte extent required for both current and next nonempty scatter payloads.
/// @details Four KiB retains the positive cold-discontinuous region and excludes the 256/512-byte hot regressions.
///          Concat converts this byte bound to character counts by division, never by an overflowing multiplication.
inline constexpr ::std::size_t concat_scatter_chain_read_prfch_minimum_payload_bytes{4u * 1024u};

/// @brief Historical x86 cache intent retained by the concat scatter-chain read policy.
inline constexpr ::fast_io::prfch_level concat_scatter_chain_read_prfch_level{
	::fast_io::prfch_level::L1};

/// @brief Historical x86 retention intent retained by the concat scatter-chain read policy.
inline constexpr ::fast_io::prfch_retention concat_scatter_chain_read_prfch_retention{
	::fast_io::prfch_retention::keep};

/// @brief Identifies a target/tune pair eligible only for concat's materialized scatter-chain read site.
template <typename platform_type>
concept concat_scatter_chain_read_prfch_platform =
	::fast_io::prfch_platform<platform_type> &&
	::fast_io::details::concat_scatter_chain_read_prfch_platform_impl<platform_type>();

/// @brief Independently named write-direction gate for the same concat site; initially always false.
template <typename platform_type>
concept concat_scatter_chain_write_prfch_platform =
	::fast_io::prfch_platform<platform_type> &&
	::fast_io::details::concat_scatter_chain_write_prfch_platform_impl<platform_type>();

/// @brief Proves the compile-time premises for concat's retained scatter-chain read-prefetch strategy.
/// @details Every `source_type` is the normalized object whose descriptor remains live through the materialized copy.
///          Each source must either carry explicit read provenance or prove that it exposes no external range at all;
///          this admits `io_null_t` without turning it into a hint target. One unmarked raw scatter, pointer, view, or
///          device mapping still closes the complete strategy. Descriptor capacity is only an upper bound for
///          reserve-scatters; the existing size pass independently checks the platform policy's complete actual-count
///          and payload domains before the copy helper inspects any payload address.
template <typename platform_type, ::std::size_t descriptor_capacity, typename... source_types>
concept concat_scatter_chain_read_prfch_strategy =
	::fast_io::concat_scatter_chain_read_prfch_platform<platform_type> &&
	descriptor_capacity >=
		::fast_io::concat_scatter_chain_read_prfch_policy_for<platform_type>.minimum_descriptor_count &&
	sizeof...(source_types) != 0u &&
	(::fast_io::prfch_cacheable_read_or_no_external_range<source_types> && ...);

/// @brief Write-provenance counterpart retained as a separate, currently disabled strategy surface.
template <typename platform_type, ::std::size_t descriptor_capacity, typename... destination_types>
concept concat_scatter_chain_write_prfch_strategy =
	::fast_io::concat_scatter_chain_write_prfch_platform<platform_type> &&
	descriptor_capacity >= ::fast_io::concat_scatter_chain_read_prfch_minimum_descriptor_count &&
	sizeof...(destination_types) != 0u &&
	(::fast_io::prfch_cacheable_write_provenance<destination_types> && ...);

namespace details
{

/// @brief Selects the target envelope for print's full-output scatter materializer.
/// @details The predicate deliberately preserves concat's evidence-bounded hybrid allow-list because this print site
///          performs the identical memory operation: irregular retained cacheable sources are copied into one
///          contiguous destination before a single output commit. A separate P-core boundary-cost check and a
///          three-process E-core confirmation found no 4/16-KiB hot/cold regression after the large-copy outline. Print
///          nevertheless keeps its own predicate, constants, and public concept so future site-specific evidence can
///          change this decision without silently widening concat. Untested Intel Core and AMD Zen tunes remain off.
///          Apple AArch64 also remains off: the corrected M4 P-core kernel experiment found a repeatable but
///          differently bounded 512--1024-descriptor, 512--3072-byte L1/keep window, the E-core follow-up did not
///          preserve the complete rectangle, `arm_apple` covers unmeasured M1--M5 products, and no complete public
///          print specialization has confirmed that window.
template <typename platform_type>
inline consteval bool print_scatter_materialize_read_prfch_platform_impl() noexcept
{
	if constexpr (!::fast_io::data_prfch_platform<platform_type>)
	{
		return false;
	}
	else
	{
		constexpr auto isa{static_cast<::fast_io::prfch_isa>(platform_type::isa)};
		constexpr auto tune{static_cast<::fast_io::prfch_tune>(platform_type::tune)};
		return isa == ::fast_io::prfch_isa::x86 &&
			   tune == ::fast_io::prfch_tune::x86_intel_hybrid;
	}
}

/// @brief Verifies cacheable-read provenance for exactly the first N normalized print sources.
/// @details `print_controls_impl` passes the unconsumed tail after the selected scatter prefix to the same helper.
///          Stopping at `remaining == 0` is therefore semantically important: a later generic printable must neither
///          be queried nor close an otherwise proved prefix. Conversely, exhausting the type list early rejects the
///          strategy so a descriptor capacity cannot manufacture missing source proofs.
template <::std::size_t remaining>
inline consteval bool print_scatter_materialize_first_n_cacheable_impl() noexcept
{
	return remaining == 0u;
}

template <::std::size_t remaining, typename source_type, typename... source_types>
inline consteval bool print_scatter_materialize_first_n_cacheable_impl() noexcept
{
	if constexpr (remaining == 0u)
	{
		return true;
	}
	else if constexpr (!::fast_io::prfch_cacheable_read_or_no_external_range<source_type>)
	{
		return false;
	}
	else
	{
		return ::fast_io::details::print_scatter_materialize_first_n_cacheable_impl<
			remaining - 1u, source_types...>();
	}
}

} // namespace details

/// @brief Minimum number of argument positions and retained payload descriptors considered by print's read hint.
/// @details Requiring both quantities prevents null-only argument positions and the optional println descriptor from
///          satisfying the static trip-count premise. The run-time size pass independently requires 32 nonempty source
///          payloads, so a customization returning an empty range cannot satisfy the strategy through capacity alone.
inline constexpr ::std::size_t print_scatter_materialize_read_prfch_minimum_descriptor_count{32u};

/// @brief Minimum byte extent of every nonempty source in an admitted print materialization.
/// @details This independent name currently retains concat's measured four-KiB boundary. The print implementation
///          converts it to character counts without multiplication, preserving the bound for wide character types and
///          adversarial lengths without introducing size_t overflow.
inline constexpr ::std::size_t print_scatter_materialize_read_prfch_minimum_payload_bytes{4u * 1024u};

/// @brief Cache level requested by print's measured next-source read hint.
inline constexpr ::fast_io::prfch_level print_scatter_materialize_read_prfch_level{
	::fast_io::prfch_level::L1};

/// @brief Retention policy requested by print's measured next-source read hint.
inline constexpr ::fast_io::prfch_retention print_scatter_materialize_read_prfch_retention{
	::fast_io::prfch_retention::keep};

/// @brief Identifies a target/tune pair eligible only for print's full-output scatter materializer.
template <typename platform_type>
concept print_scatter_materialize_read_prfch_platform =
	::fast_io::prfch_platform<platform_type> &&
	::fast_io::details::print_scatter_materialize_read_prfch_platform_impl<platform_type>();

/// @brief Proves the compile-time premises for print's retained scatter materialization read hint.
/// @details `position` names exactly the leading argument prefix selected by the print dispatcher, while
///          `descriptor_capacity` excludes both null positions and println's synthetic newline. Every normalized source
///          in that prefix must carry explicit cacheable-read provenance or be the vacuous `io_null_t`; raw scatter
///          descriptors, pointers, views, devices, and unproved composite leaves close the strategy. These premises do
///          not inspect an address. The existing size traversal must still prove 32 nonempty, repeatable, live payloads
///          of at least four KiB before the materializer may issue a hint.
template <typename platform_type, ::std::size_t position, ::std::size_t descriptor_capacity,
		  typename... source_types>
concept print_scatter_materialize_read_prfch_strategy =
	::fast_io::print_scatter_materialize_read_prfch_platform<platform_type> &&
	position >= ::fast_io::print_scatter_materialize_read_prfch_minimum_descriptor_count &&
	descriptor_capacity >= ::fast_io::print_scatter_materialize_read_prfch_minimum_descriptor_count &&
	::fast_io::details::print_scatter_materialize_first_n_cacheable_impl<position, source_types...>();

namespace details
{

/// @brief Keeps prefetch before a contiguous scan-consumption CPO disabled on every target.
/// @details A paired Linux x86 experiment placed one bounded L1/keep read hint immediately before an otherwise
///          identical branchy token scanner. Across 4 KiB through 1 MiB extents, hot and 128 MiB discontinuous-cold
///          inputs, and three independent data orders, the candidate repeatedly changed sign within measurement
///          noise (approximately -0.35 through +0.37 percent). This is expected for a forward sequential consumer: the
///          hardware stream prefetcher already sees the access pattern, while the generic dispatcher cannot know how
///          many bytes an arbitrary precise, contiguous, context, or terminal scanner will consume before the hinted
///          line.
///
///          The all-false result is therefore an evidence-bearing policy, not an omitted implementation. A future
///          target/tune exception must retain a scanner-independent distance, prove a hot-path win as well as a cold
///          win, and remain separate from scatter-copy policies whose irregular next range supplies useful lead time.
template <typename platform_type>
inline consteval bool scan_contiguous_consume_read_prfch_platform_impl() noexcept
{
	return false;
}

/// @brief Keeps write prefetch before an owned input-buffer refill disabled on every target.
/// @details The independent refill experiment issued one bounded L1/keep write hint before the same `/dev/zero` read.
///          Rotating among cold destinations improved 4 KiB transfers by roughly 5--10 percent and 16 KiB transfers by
///          roughly 1--5 percent, but that access pattern is not the owned-buffer contract: normal underflow repeatedly
///          refills one allocation. In the corresponding hot steady state no distance retained a win, and regressions
///          reached roughly 1.1 percent. Enabling the common refill path from the cold result would charge that cost on
///          every later underflow. Allocation also proves neither committed pages nor a resident translation, so the
///          first refill cannot safely be equated with the experiment's already resident cold pages.
///
///          Read-consumption and refill-write decisions stay separate because their instructions, memory directions,
///          and amortization differ. A future one-shot allocation-state design can benchmark and enable this predicate
///          without silently authorizing a repeated refill hint.
template <typename platform_type>
inline consteval bool scan_owned_refill_write_prfch_platform_impl() noexcept
{
	return false;
}

} // namespace details

/// @brief Identifies a target/tune pair admitted specifically for pre-CPO contiguous scan consumption.
/// @details This site gate is intentionally false today. Keeping it public and separately named prevents the broad
///          conservative read allow-list, or a successful concat/print experiment, from forcing a hint into scan.
template <typename platform_type>
concept scan_contiguous_consume_read_prfch_platform =
	::fast_io::prfch_platform<platform_type> &&
	::fast_io::details::scan_contiguous_consume_read_prfch_platform_impl<platform_type>();

/// @brief Combines scan-consumption site evidence with explicit provenance for the normalized input owner.
/// @details `input_owner_type` is the object whose live get area supplies the scanner's current span; it is not the
///          semantic manipulator being populated. Width, conditional, alias, pack, and other scan manipulators describe
///          parsing or destination semantics and cannot prove the memory domain of the input cursor. Even after this
///          concept is enabled, a call site must form only an in-range address, keep the range alive through the hint,
///          preserve constant-evaluation semantics by issuing no instruction there, and apply the retained run-time
///          extent threshold. Raw pointers, buffer views, and scatter descriptors remain ineligible by default.
template <typename platform_type, typename input_owner_type>
concept scan_contiguous_consume_read_prfch_strategy =
	::fast_io::scan_contiguous_consume_read_prfch_platform<platform_type> &&
	::fast_io::prfch_cacheable_read_provenance<input_owner_type>;

/// @brief Identifies a target/tune pair admitted specifically for writes into an owned refill buffer.
/// @details This independently extensible gate is false today because the measured hot steady state regressed.
template <typename platform_type>
concept scan_owned_refill_write_prfch_platform =
	::fast_io::prfch_platform<platform_type> &&
	::fast_io::details::scan_owned_refill_write_prfch_platform_impl<platform_type>();

/// @brief Combines refill-site evidence with an explicit ordinary-cacheable write promise from the buffer owner.
/// @details Mutable pointer syntax and successful allocation are insufficient: custom allocators may expose device or
///          persistent memory, and `basic_io_buffer_pointers` carries bounds but no ownership-domain proof. The owner or
///          allocator-backed wrapper must opt in independently. If this site is enabled later, the implementation must
///          additionally distinguish first allocation from repeated underflow and prove the hinted byte is inside the
///          writable live allocation before issuing a run-time-only write hint.
template <typename platform_type, typename buffer_owner_type>
concept scan_owned_refill_write_prfch_strategy =
	::fast_io::scan_owned_refill_write_prfch_platform<platform_type> &&
	::fast_io::prfch_cacheable_write_provenance<buffer_owner_type>;

} // namespace fast_io
