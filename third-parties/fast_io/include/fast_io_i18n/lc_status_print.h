#pragma once

namespace fast_io
{

/// @brief Recognizes an object-dependent locale reserve producer.
/// @details The size result is a capacity bound; the returned pointer is the exact committed end and may precede that
///          bound. Accepting merely convertible protocol results would admit proxy values whose conversions can
///          disagree between capability detection and the selected call expression.
template <typename char_type, typename T>
concept lc_dynamic_reserve_printable =
	::std::integral<char_type> &&
	requires(T t, ::fast_io::basic_lc_all<char_type> const *lc, char_type *ptr) {
		{ print_reserve_size(lc, t) } -> ::std::same_as<::std::size_t>;
		{ print_reserve_define(lc, ptr, t) } -> ::std::same_as<char_type *>;
	};

template <::std::integral char_type, typename value_type>
	requires lc_dynamic_reserve_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::size_t
print_reserve_size(::fast_io::basic_lc_all<char_type> const *lc, parameter<value_type> &para)
{
	return print_reserve_size(lc, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires lc_dynamic_reserve_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr char_type *
print_reserve_define(::fast_io::basic_lc_all<char_type> const *lc, char_type *begin,
					 parameter<value_type> &para)
{
	return print_reserve_define(lc, begin, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires lc_dynamic_reserve_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr ::std::size_t
print_reserve_size(::fast_io::basic_lc_all<char_type> const *lc,
				   parameter<value_type> const &para)
{
	// Constness belongs to the normalized owner, not to the transport wrapper. Mirror the exact member expression so
	// an owning const formatter cannot regain mutable access through a locale adapter.
	return print_reserve_size(lc, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires lc_dynamic_reserve_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr char_type *
print_reserve_define(::fast_io::basic_lc_all<char_type> const *lc, char_type *begin,
					 parameter<value_type> const &para)
{
	return print_reserve_define(lc, begin, para.reference);
}

/// @brief Recognizes a locale customization that returns a borrowed character scatter.
template <typename char_type, typename T>
concept lc_scatter_printable =
	::std::integral<char_type> &&
	requires(::fast_io::basic_lc_all<char_type> const *lc, T t) {
		{ print_scatter_define(lc, t) } -> ::std::same_as<basic_io_scatter_t<char_type>>;
	};

template <::std::integral char_type, typename value_type>
	requires lc_scatter_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr basic_io_scatter_t<char_type>
print_scatter_define(::fast_io::basic_lc_all<char_type> const *lc, parameter<value_type> &para)
{
	return print_scatter_define(lc, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires lc_scatter_printable<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr basic_io_scatter_t<char_type>
print_scatter_define(::fast_io::basic_lc_all<char_type> const *lc, parameter<value_type> const &para)
{
	return print_scatter_define(lc, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires lc_scatter_printable<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr basic_io_scatter_t<char_type>
print_scatter_define(::fast_io::basic_lc_all<char_type> const *lc, parameter<value_type> &&para)
{
	// Bind the caller's wrapper for the duration of the full expression. Copying an owning parameter here could return
	// a descriptor into an adapter-local member destroyed before the caller consumes the scatter.
	return print_scatter_define(lc, para);
}

/// @brief Opts a locale scatter source into retained, repeatable descriptor composition.
/// @details A locale scatter CPO proves only pointer/length shape. The pointed storage may be a shared conversion
///          scratch area, and even two independently long-lived buffers may be selected on alternating observations.
///          This explicit marker therefore promises both that every returned character range remains valid through the
///          enclosing print and that observing the same unchanged source under the same locale again yields the same
///          length and character sequence. Ordinary retained coalescers rely on the latter property when they measure
///          and later materialize a run. Without the marker the immediate single-value path remains valid.
template <typename char_type, typename T>
concept lc_borrowed_scatter_source =
	::std::integral<char_type> && lc_scatter_printable<char_type, T> && requires {
		{
			print_lc_borrowed_scatter_source(
				io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

template <::std::integral char_type, typename value_type>
	requires ::fast_io::lc_borrowed_scatter_source<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::true_type print_lc_borrowed_scatter_source(
	io_reserve_type_t<char_type, parameter<value_type>>) noexcept
{
	// `parameter` either owns the producer for the complete operation or preserves the caller's exact reference. It
	// therefore propagates an existing locale lifetime-and-repeatability proof without manufacturing one.
	return {};
}

/// @brief Preserves the historical dummy-destination classification of locale direct printers.
/// @details This public compatibility concept proves only the traditional customization shape. Dispatch never uses it
///          as proof that the same overload accepts a concrete file, decorator, or string output.
template <typename char_type, typename T>
concept lc_printable =
	::std::integral<char_type> &&
	requires(::fast_io::basic_lc_all<char_type> const *lc,
			 ::fast_io::details::dummy_buffer_output_stream<char_type> out, T t) {
		{ print_define(lc, out, t) } -> ::std::same_as<void>;
	};

namespace details
{

/// @brief Proves the exact locale direct-print expression used by dispatch.
/// @details A dummy-only overload makes `lc_printable` true but cannot print to a real destination; conversely, an
///          output-specific overload can be valid while the historical dummy probe is false. The output and argument
///          parameters are named lvalues here, exactly matching the locals used by the dispatcher.
template <typename char_type, typename output, typename T>
concept lc_direct_printable_to =
	::std::integral<char_type> &&
	requires(::fast_io::basic_lc_all<char_type> const *lc, output out, T t) {
		{ print_define(lc, out, t) } -> ::std::same_as<void>;
	};

} // namespace details

template <typename output, typename value_type>
	requires requires {
		typename output::output_char_type;
} && ::fast_io::details::lc_direct_printable_to<
	typename output::output_char_type, output,
	::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr void print_define(
	::fast_io::basic_lc_all<typename output::output_char_type> const *lc, output &out,
	::fast_io::parameter<value_type> &para)
{
	// Both objects have already crossed their ownership boundary. Borrowing them preserves an inline observer cursor
	// and an owning formatter's identity; the underlying locale CPO remains free to request value transport explicitly.
	print_define(lc, out, para.reference);
}

template <typename output, typename value_type>
	requires requires {
		typename output::output_char_type;
} && ::fast_io::details::lc_direct_printable_to<
	typename output::output_char_type, output,
	::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr void print_define(
	::fast_io::basic_lc_all<typename output::output_char_type> const *lc, output &out,
	::fast_io::parameter<value_type> const &para)
{
	// The destination is already normalized and the const wrapper is the sole formatter owner. Borrowing both makes
	// the adapter's requires-expression and executed call use the same cv/ref categories.
	print_define(lc, out, para.reference);
}

template <typename char_type, typename T>
concept lc_printable_internal_shift =
	::std::integral<char_type> &&
	requires(::fast_io::basic_lc_all<char_type> const *lc, T t) {
		{ print_define_internal_shift(lc, t) } -> ::std::same_as<::std::size_t>;
	};

template <::std::integral char_type, typename value_type>
	requires lc_printable_internal_shift<
		char_type, ::fast_io::details::parameter_mutable_member_reference_t<value_type>>
inline constexpr ::std::size_t
print_define_internal_shift(::fast_io::basic_lc_all<char_type> const *lc, parameter<value_type> &para)
{
	return print_define_internal_shift(lc, para.reference);
}

template <::std::integral char_type, typename value_type>
	requires lc_printable_internal_shift<
		char_type, ::fast_io::details::parameter_const_member_reference_t<value_type>>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::basic_lc_all<char_type> const *lc, parameter<value_type> const &para)
{
	return print_define_internal_shift(lc, para.reference);
}

namespace details::decay
{

/// @brief Describes locale or ordinary values that can be materialized into one contiguous character range.
/// @details This is intentionally narrower than general printability. Direct printers are output-specific and cannot
///          participate in a reserve calculation without first choosing a destination. A raw locale scatter is also
///          insufficient: pack and condition adapters call size and materialization separately, so only the explicit
///          lifetime-and-repeatability marker proves that both observations have equal length and bytes. This applies
///          equally to an ordinary scatter used as one arm of a locale semantic node. Unmarked scatters remain
///          available to their immediate single-leaf bridges and never enter this synthesized two-pass plan.
template <typename char_type, typename T>
concept lc_contiguous_printable =
	::std::integral<char_type> &&
	((::fast_io::lc_scatter_printable<char_type, T> &&
	  ::fast_io::lc_borrowed_scatter_source<char_type, T>) ||
	::fast_io::lc_dynamic_reserve_printable<char_type, T> ||
	(::fast_io::scatter_printable_for<char_type, T> &&
	 ::fast_io::borrowed_scatter_source<char_type, ::std::remove_cvref_t<T>>) ||
	::fast_io::reserve_printable<char_type, T> ||
	::fast_io::dynamic_reserve_printable<char_type, T>);

/// @brief Measures one contiguous locale-or-ordinary value using the same protocol priority as emission.
template <::std::integral char_type, typename T>
	requires lc_contiguous_printable<char_type, T>
inline constexpr ::std::size_t lc_contiguous_size(
	::fast_io::basic_lc_all<char_type> const *lc, T &&value)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::lc_scatter_printable<char_type, T> &&
				  ::fast_io::lc_borrowed_scatter_source<char_type, T>)
	{
		return print_scatter_define(lc, value).len;
	}
	else if constexpr (::fast_io::lc_dynamic_reserve_printable<char_type, T>)
	{
		return print_reserve_size(lc, value);
	}
	else if constexpr (
		::fast_io::scatter_printable_for<char_type, T> &&
		::fast_io::borrowed_scatter_source<char_type, ::std::remove_cvref_t<T>>)
	{
		return print_scatter_define(::fast_io::io_reserve_type<char_type, value_type>, value).len;
	}
	else if constexpr (::fast_io::reserve_printable<char_type, T>)
	{
		return print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>);
	}
	else
	{
		return print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>, value);
	}
}

/// @brief Materializes one previously measured contiguous value and returns the exact committed end.
template <::std::integral char_type, typename T>
	requires lc_contiguous_printable<char_type, T>
inline constexpr char_type *lc_contiguous_define(
	::fast_io::basic_lc_all<char_type> const *lc, char_type *destination, T &&value)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::lc_scatter_printable<char_type, T> &&
				  ::fast_io::lc_borrowed_scatter_source<char_type, T>)
	{
		auto const scatter{print_scatter_define(lc, value)};
		return ::fast_io::details::non_overlapped_copy_n(scatter.base, scatter.len, destination);
	}
	else if constexpr (::fast_io::lc_dynamic_reserve_printable<char_type, T>)
	{
		return print_reserve_define(lc, destination, value);
	}
	else if constexpr (
		::fast_io::scatter_printable_for<char_type, T> &&
		::fast_io::borrowed_scatter_source<char_type, ::std::remove_cvref_t<T>>)
	{
		auto const scatter{
			print_scatter_define(::fast_io::io_reserve_type<char_type, value_type>, value)};
		return ::fast_io::details::non_overlapped_copy_n(scatter.base, scatter.len, destination);
	}
	else
	{
		return print_reserve_define(
			::fast_io::io_reserve_type<char_type, value_type>, destination, value);
	}
}

template <::std::integral char_type>
inline constexpr ::std::size_t lc_contiguous_size_sum(
	::fast_io::basic_lc_all<char_type> const *) noexcept
{
	return 0u;
}

template <::std::integral char_type, typename T, typename... Args>
inline constexpr ::std::size_t lc_contiguous_size_sum(
	::fast_io::basic_lc_all<char_type> const *lc, T &value, Args &...args)
{
	// These are already named children of the pack's sole normalized owner. Re-running status forwarding in both the
	// size and materialization passes could select two different proxies and violates the one-decay invariant. Evaluate
	// the head before entering the recursive tail as well: function-argument evaluation order cannot prove observable
	// producer order, whereas these separate statements do.
	auto const head_size{::fast_io::details::decay::lc_contiguous_size<char_type>(lc, value)};
	auto const tail_size{
		::fast_io::details::decay::lc_contiguous_size_sum<char_type>(lc, args...)};
	return ::fast_io::details::intrinsics::add_or_overflow_die(head_size, tail_size);
}

template <::std::integral char_type>
inline constexpr char_type *lc_contiguous_define_sum(
	::fast_io::basic_lc_all<char_type> const *, char_type *destination) noexcept
{
	return destination;
}

template <::std::integral char_type, typename T, typename... Args>
inline constexpr char_type *lc_contiguous_define_sum(
	::fast_io::basic_lc_all<char_type> const *lc, char_type *destination, T &value,
	Args &...args)
{
	auto next{::fast_io::details::decay::lc_contiguous_define<char_type>(lc, destination, value)};
	return ::fast_io::details::decay::lc_contiguous_define_sum<char_type>(
		lc, next, args...);
}

template <::std::integral char_type>
struct lc_pack_measure_continuation
{
	::fast_io::basic_lc_all<char_type> const *lc;

	template <typename... Args>
	inline constexpr ::std::size_t operator()(Args &...args) const
	{
		return ::fast_io::details::decay::lc_contiguous_size_sum<char_type>(
			lc, args...);
	}
};

template <::std::integral char_type>
struct lc_pack_materialize_continuation
{
	::fast_io::basic_lc_all<char_type> const *lc;
	char_type *destination;

	template <typename... Args>
	inline constexpr char_type *operator()(Args &...args) const
	{
		return ::fast_io::details::decay::lc_contiguous_define_sum<char_type>(
			lc, destination, args...);
	}
};

} // namespace details::decay

/// @brief Gives a nested semantic pack a locale-aware contiguous protocol when every stored child has one.
/// @details Top-level packs are flattened before dispatch. This protocol exists for packs nested inside width or
///          condition nodes, where flattening would change the parent's semantics. Every child is already a named
///          object in the pack's normalized storage, so both passes call its exact lvalue protocol without re-running
///          alias or status forwarding. Locale scatter children additionally carry the explicit retained/repeatable
///          marker; otherwise this synthesized two-pass protocol is not advertised at all.
template <::std::integral char_type, typename... Args>
	requires((::fast_io::details::decay::lc_contiguous_printable<
			  char_type, Args &>) && ...)
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::basic_lc_all<char_type> const *lc, ::fast_io::manipulators::pack_t<Args...> &pack)
{
	// The semantic engine already owns or borrows this pack. Applying the two locale passes to the same named object
	// preserves move-only children and prevents identity/state from diverging between measurement and materialization.
	return ::fast_io::details::decay::print_semantic_pack_apply(
		pack, ::fast_io::details::decay::lc_pack_measure_continuation<char_type>{lc});
}

template <::std::integral char_type, typename... Args>
	requires((::fast_io::details::decay::lc_contiguous_printable<
			  char_type, Args &>) && ...)
inline constexpr char_type *print_reserve_define(
	::fast_io::basic_lc_all<char_type> const *lc, char_type *destination,
	::fast_io::manipulators::pack_t<Args...> &pack)
{
	return ::fast_io::details::decay::print_semantic_pack_apply(
		pack,
		::fast_io::details::decay::lc_pack_materialize_continuation<char_type>{lc, destination});
}

namespace details::decay
{

/// @brief A zero-owning bridge from locale protocols to the current ordinary print strategy engine.
/// @details The bridge stores pointers only. Its referent is an argument in the synchronous normalization chain and
///          the locale aggregate is owned by the enclosing imbuer, so both outlive measurement, descriptor planning,
///          and emission. This lets locale reserve/scatter/direct CPOs reuse the maintained stack, heap, buffering,
///          semantic, and syscall strategies instead of duplicating a second dispatcher that can drift out of date.
template <::std::integral char_type, typename T>
struct lc_bound_printable
{
	::fast_io::basic_lc_all<char_type> const *lc;
	T *value;
};

/// @brief Owns a character-forwarding result whose locale protocol must survive ordinary semantic normalization.
/// @details A nested pack stores aliases before its output character type is known. Its later status-forward CPO may
///          therefore produce a move-only locale-only proxy. A pointer bridge to the forwarding helper's local proxy
///          would dangle; this sibling moves the proxy into the ordinary strategy graph and delegates locale CPOs from
///          that stable owner. Stable lvalue results continue to use `lc_bound_printable` instead.
template <::std::integral char_type, typename T>
struct lc_owned_bound_printable
{
	::fast_io::basic_lc_all<char_type> const *lc;
	T value;
};

/// @brief Defers character-dependent forwarding of one raw nested-pack alias until ordinary pack expansion.
/// @details The wrapper itself is a two-pointer view into the enclosing locale and semantic owner. Its status CPO
///          evaluates the wrapped source exactly once, then returns either an owned locale bridge or a bridge to an
///          independently stable lvalue proxy. This preserves locale context without calling a stateful forwarder once
///          during reserve sizing and again during materialization.
template <::std::integral char_type, typename output, typename T>
struct lc_deferred_locale_forward
{
	::fast_io::basic_lc_all<char_type> const *lc;
	T *value;
};

} // namespace details::decay

template <::std::integral char_type, typename T>
	requires ::fast_io::lc_dynamic_reserve_printable<char_type, T &>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::details::decay::lc_bound_printable<char_type, T>>,
	::fast_io::details::decay::lc_bound_printable<char_type, T> bound)
{
	return print_reserve_size(bound.lc, *bound.value);
}

template <::std::integral char_type, typename T>
	requires ::fast_io::lc_dynamic_reserve_printable<char_type, T &>
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::details::decay::lc_bound_printable<char_type, T>>,
	char_type *destination,
	::fast_io::details::decay::lc_bound_printable<char_type, T> bound)
{
	return print_reserve_define(bound.lc, destination, *bound.value);
}

template <::std::integral char_type, typename T>
	requires ::fast_io::lc_scatter_printable<char_type, T &>
inline constexpr basic_io_scatter_t<char_type> print_scatter_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::details::decay::lc_bound_printable<char_type, T>>,
	::fast_io::details::decay::lc_bound_printable<char_type, T> bound)
{
	return print_scatter_define(bound.lc, *bound.value);
}

/// @brief Propagates an explicit locale scatter lifetime-and-repeatability proof through the ordinary print bridge.
/// @details Keeping the locale aggregate and source object alive does not by itself keep storage returned by their CPO
///          stable, nor does it make a second observation return the same sequence. The bridge is therefore borrowed
///          only when the original locale source independently promises the complete ordinary retained-scatter
///          contract; weakening that promise here would make two-pass range/concat sizing unsound.
template <::std::integral char_type, typename T>
	requires ::fast_io::lc_borrowed_scatter_source<char_type, T &>
inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::details::decay::lc_bound_printable<char_type, T>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename output, typename T>
	requires ::fast_io::details::lc_direct_printable_to<char_type, output, T &>
inline constexpr void print_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::details::decay::lc_bound_printable<char_type, T>>,
	output &out, ::fast_io::details::decay::lc_bound_printable<char_type, T> bound)
{
	// `lc_bound_printable` owns only locale/source pointers; it must not reopen the already-normalized output observer's
	// ownership boundary. An identity-bearing observer is borrowed exactly as it is by ordinary direct-print dispatch.
	print_define(bound.lc, out, *bound.value);
}

/// @brief Bridges locale internal-placement metadata into the ordinary semantic width engine.
/// @details Width transformation replaces a locale leaf with `lc_bound_printable`. Propagating the shift through the
///          same bridge lets the maintained internal-padding implementation insert fill after a sign or prefix without
///          retaining the removed locale-specific width algorithm.
template <::std::integral char_type, typename T>
	requires ::fast_io::lc_printable_internal_shift<char_type, T &>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::details::decay::lc_bound_printable<char_type, T>>,
	::fast_io::details::decay::lc_bound_printable<char_type, T> bound)
{
	return print_define_internal_shift(bound.lc, *bound.value);
}

template <::std::integral char_type, typename T>
	requires ::fast_io::lc_dynamic_reserve_printable<char_type, T &>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::details::decay::lc_owned_bound_printable<char_type, T>>,
	::fast_io::details::decay::lc_owned_bound_printable<char_type, T> &bound)
{
	return print_reserve_size(bound.lc, bound.value);
}

template <::std::integral char_type, typename T>
	requires ::fast_io::lc_dynamic_reserve_printable<char_type, T &>
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::details::decay::lc_owned_bound_printable<char_type, T>>,
	char_type *destination,
	::fast_io::details::decay::lc_owned_bound_printable<char_type, T> &bound)
{
	return print_reserve_define(bound.lc, destination, bound.value);
}

template <::std::integral char_type, typename T>
	requires ::fast_io::lc_scatter_printable<char_type, T &>
inline constexpr basic_io_scatter_t<char_type> print_scatter_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::details::decay::lc_owned_bound_printable<char_type, T>>,
	::fast_io::details::decay::lc_owned_bound_printable<char_type, T> &bound)
{
	return print_scatter_define(bound.lc, bound.value);
}

template <::std::integral char_type, typename T>
	requires ::fast_io::lc_borrowed_scatter_source<char_type, T &>
inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::details::decay::lc_owned_bound_printable<char_type, T>>) noexcept
{
	// Owning the proxy supplies object lifetime, while the propagated locale marker independently supplies stable,
	// repeatable descriptor bytes. Neither fact is inferred from the other.
	return {};
}

template <::std::integral char_type, typename output, typename T>
	requires ::fast_io::details::lc_direct_printable_to<char_type, output, T &>
inline constexpr void print_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::details::decay::lc_owned_bound_printable<char_type, T>>,
	output &out,
	::fast_io::details::decay::lc_owned_bound_printable<char_type, T> &bound)
{
	print_define(bound.lc, out, bound.value);
}

template <::std::integral char_type, typename T>
	requires ::fast_io::lc_printable_internal_shift<char_type, T &>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::details::decay::lc_owned_bound_printable<char_type, T>>,
	::fast_io::details::decay::lc_owned_bound_printable<char_type, T> &bound)
{
	return print_define_internal_shift(bound.lc, bound.value);
}

namespace details::decay
{

template <::std::integral char_type, typename output, typename T>
inline constexpr bool lc_bind_printable_to_output =
	::fast_io::lc_dynamic_reserve_printable<char_type, T &> ||
	::fast_io::lc_scatter_printable<char_type, T &> ||
	::fast_io::details::lc_direct_printable_to<char_type, output, T &>;

template <typename T>
using lc_bound_storage_t = ::std::conditional_t<
	::std::is_lvalue_reference_v<T>, T, ::std::remove_cvref_t<T>>;

template <::std::integral char_type, typename forwarded_type>
inline constexpr auto lc_own_forwarded_locale_leaf(
	::fast_io::basic_lc_all<char_type> const *lc, forwarded_type &&forwarded)
{
	using value_type = ::std::remove_cvref_t<forwarded_type>;
	if constexpr (::fast_io::details::decay::print_semantic_parameter_object_v<value_type>)
	{
		if constexpr (::std::is_reference_v<decltype(forwarded.reference)>)
		{
			// A parameter carrying a language reference denotes storage whose lifetime was proved by status-forward
			// admission. Point directly at that proxy; moving the small parameter cannot extend or change its lifetime.
			using bound_value_type = ::std::remove_reference_t<decltype(forwarded.reference)>;
			return ::fast_io::details::decay::lc_bound_printable<char_type, bound_value_type>{
				lc, __builtin_addressof(forwarded.reference)};
		}
		else
		{
			return ::fast_io::details::decay::lc_owned_bound_printable<char_type, value_type>{
				lc, ::std::forward<forwarded_type>(forwarded)};
		}
	}
	else
	{
		// A prvalue status result belongs to this forwarding frame. Move it into the returned bridge so both the reserve
		// and define operations observe the same owner, including for a noncopyable proxy.
		return ::fast_io::details::decay::lc_owned_bound_printable<char_type, value_type>{
			lc, ::std::forward<forwarded_type>(forwarded)};
	}
}

template <typename forwarded_type, bool parameter =
	::fast_io::details::decay::print_semantic_parameter_object_v<
		::std::remove_cvref_t<forwarded_type>>>
struct lc_forwarded_locale_expression
{
	using type = ::std::remove_reference_t<forwarded_type> &;
};

template <typename forwarded_type>
struct lc_forwarded_locale_expression<forwarded_type, true>
{
	using type = ::fast_io::details::parameter_mutable_member_reference_t<
		::std::remove_cvref_t<forwarded_type>>;
};

template <typename forwarded_type>
using lc_forwarded_locale_expression_t =
	typename ::fast_io::details::decay::lc_forwarded_locale_expression<forwarded_type>::type;

/// @brief Detects a raw nested alias whose one character-forwarding result needs locale binding.
/// @details The explicit status-forward CPO retains its ordinary priority even when the raw alias also has a locale
///          protocol. This predicate targets the otherwise lost context: its non-semantic forwarded leaf is locale-
///          printable, but ordinary expansion no longer knows the locale pointer. Semantic forwarding results remain
///          in the ordinary recursive semantic pipeline and are not misclassified as scalar leaves.
template <::std::integral char_type, typename output, typename T>
inline constexpr bool lc_nested_deferred_locale_forward_v = []() constexpr {
	if constexpr (!requires(T &value) {
		::fast_io::io_print_alias(value);
	})
	{
		return false;
	}
	else
	{
		using alias_type = decltype(::fast_io::io_print_alias(::std::declval<T &>()));
		if constexpr (!::fast_io::status_io_print_forwardable<char_type, alias_type>)
		{
			// Without a character-dependent forwarding CPO, direct locale binding is both cheaper and more faithful to
			// borrowed-source identity than materializing another transport value.
			return false;
		}
		else
		{
			using forwarded_type = decltype(
				::fast_io::details::decay::print_semantic_input_forward<char_type>(
					::std::declval<T &>()));
			if constexpr (::fast_io::details::decay::print_semantic_node<forwarded_type>)
			{
				return false;
			}
			else
			{
				using expression_type =
					::fast_io::details::decay::lc_forwarded_locale_expression_t<forwarded_type>;
				return ::fast_io::details::decay::lc_bind_printable_to_output<
					char_type, output, expression_type>;
			}
		}
	}
}();

template <::std::integral char_type, typename output, typename T>
inline constexpr decltype(auto) lc_bind_one(
	::fast_io::basic_lc_all<char_type> const *lc, T &value);

template <::std::integral char_type, typename output, typename T>
inline constexpr decltype(auto) lc_bind_nested_one(
	::fast_io::basic_lc_all<char_type> const *lc, T &value)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (
		!::fast_io::details::decay::print_semantic_node<value_type> &&
		::fast_io::details::decay::lc_nested_deferred_locale_forward_v<
			char_type, output, T>)
	{
		return ::fast_io::details::decay::lc_deferred_locale_forward<char_type, output, T>{
			lc, __builtin_addressof(value)};
	}
	else
	{
		return ::fast_io::details::decay::lc_bind_one<char_type, output>(lc, value);
	}
}

/// @brief Rebuilds a static-placement width node around its locale-bound child.
/// @details A value bridge is stored by value; an unchanged normalized child remains a reference. This is the same
///          lifetime split used by the semantic manipulators themselves and prevents a returned width node from
///          retaining a reference to a temporary bridge local.
template <::fast_io::manipulators::scalar_placement placement, typename T, typename bound_type>
inline constexpr auto lc_rebind_width(
	::fast_io::manipulators::width_t<placement, T> const &node, bound_type &&bound)
{
	using storage_type = ::fast_io::details::decay::lc_bound_storage_t<bound_type &&>;
	return ::fast_io::manipulators::width_t<placement, storage_type>{
		::std::forward<bound_type>(bound), node.width};
}

template <::fast_io::manipulators::scalar_placement placement, typename T,
	::std::integral fill_char_type, typename bound_type>
inline constexpr auto lc_rebind_width(
	::fast_io::manipulators::width_ch_t<placement, T, fill_char_type> const &node,
	bound_type &&bound)
{
	using storage_type = ::fast_io::details::decay::lc_bound_storage_t<bound_type &&>;
	return ::fast_io::manipulators::width_ch_t<placement, storage_type, fill_char_type>{
		::std::forward<bound_type>(bound), node.width, node.ch};
}

template <typename T, typename bound_type>
inline constexpr auto lc_rebind_width(
	::fast_io::manipulators::width_runtime_t<T> const &node, bound_type &&bound)
{
	using storage_type = ::fast_io::details::decay::lc_bound_storage_t<bound_type &&>;
	return ::fast_io::manipulators::width_runtime_t<storage_type>{
		node.placement, ::std::forward<bound_type>(bound), node.width};
}

template <typename T, ::std::integral fill_char_type, typename bound_type>
inline constexpr auto lc_rebind_width(
	::fast_io::manipulators::width_runtime_ch_t<T, fill_char_type> const &node,
	bound_type &&bound)
{
	using storage_type = ::fast_io::details::decay::lc_bound_storage_t<bound_type &&>;
	return ::fast_io::manipulators::width_runtime_ch_t<storage_type, fill_char_type>{
		node.placement, ::std::forward<bound_type>(bound), node.width, node.ch};
}

/// @brief Recursively binds locale leaves while preserving the ordinary semantic type graph.
/// @details Packs, conditions, and width nodes are rebuilt around bound children, then handled exclusively by the
///          maintained semantic engine. This separation is important evidence for deleting the locale width formatter:
///          placement, padding, coalescing thresholds, stack policy, and buffered output now have one implementation.
///          Leaf reserve/scatter protocols are destination-independent; a direct leaf is admitted only by the concrete
///          output expression, so a dummy-only customization falls through to its ordinary protocol.
template <::std::integral char_type, typename output, typename T>
inline constexpr decltype(auto) lc_bind_one(
	::fast_io::basic_lc_all<char_type> const *lc, T &value)
{
	using value_type = ::std::remove_cvref_t<T>;
	// Keep cv-qualification on the referent: a normalized const object must produce a bridge containing
	// `T const *`, never a mutable pointer manufactured by type erasure.
	using bound_value_type = ::std::remove_reference_t<T>;
	if constexpr (::fast_io::details::decay::print_semantic_parameter_object_v<value_type>)
	{
		// `parameter` changes transport/lifetime only; bind the referenced semantic graph directly.
		return ::fast_io::details::decay::lc_bind_one<char_type, output>(lc, value.reference);
	}
	else if constexpr (::fast_io::details::print_pack<value_type>)
	{
		// A nested pack cannot be flattened here because an enclosing width owns its aggregate length. Rebuild the pack
		// with each child transformed, preserving references for existing storage and values for bridge objects.
		return ::fast_io::details::decay::print_semantic_pack_apply(
			value,
			[lc]<typename... Args>(Args &&...args) constexpr {
				using rebound_type = ::fast_io::manipulators::pack_t<
					decltype(::fast_io::details::decay::lc_bind_nested_one<char_type, output>(lc, args))...>;
				using storage_type = typename rebound_type::storage_type;
#if defined(__clang__)
				// fast_io's tuple is an EBO aggregate with one base per element. Its portable direct aggregate spelling
				// intentionally relies on brace elision; Clang's warning cannot express the pack-dependent number of base
				// braces, so suppress it at the same narrow construction boundary used by mnp::pack itself.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
				return rebound_type{storage_type{
					::fast_io::details::decay::lc_bind_nested_one<char_type, output>(lc, args)...}};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
			});
	}
	else if constexpr (::fast_io::details::decay::print_semantic_condition_v<value_type>)
	{
		// Top-level conditions are selected before binding; this branch serves conditions nested under another node.
		decltype(auto) first{
			::fast_io::details::decay::lc_bind_nested_one<char_type, output>(lc, value.t1)};
		decltype(auto) second{
			::fast_io::details::decay::lc_bind_nested_one<char_type, output>(lc, value.t2)};
		using first_type = ::fast_io::details::decay::lc_bound_storage_t<decltype(first)>;
		using second_type = ::fast_io::details::decay::lc_bound_storage_t<decltype(second)>;
		return ::fast_io::manipulators::condition<first_type, second_type>{
			value.pred, ::std::forward<decltype(first)>(first), ::std::forward<decltype(second)>(second)};
	}
	else if constexpr (::fast_io::details::decay::print_semantic_width_v<value_type>)
	{
		decltype(auto) child{
			::fast_io::details::decay::lc_bind_nested_one<char_type, output>(lc, value.reference)};
		return ::fast_io::details::decay::lc_rebind_width(
			value, ::std::forward<decltype(child)>(child));
	}
	else if constexpr (::fast_io::details::decay::lc_bind_printable_to_output<
					  char_type, output, bound_value_type>)
	{
		return ::fast_io::details::decay::lc_bound_printable<char_type, bound_value_type>{
			lc, __builtin_addressof(value)};
	}
	else
	{
		return (value);
	}
}

/// @brief Proves that one normalized locale run can be emitted by a concrete output reference.
/// @details Locale binding is output-dependent: a leaf with a direct locale CPO becomes a bridge only when that exact
///          destination accepts it, while destination-independent locale reserve/scatter leaves and unchanged ordinary
///          leaves enter the ordinary print engine. Forming the rebound types with `lc_bind_one` and then applying the
///          maintained line-aware ordinary admission predicate models precisely that two-stage protocol. In particular,
///          merely finding an `io_strlike_ref` expression is not evidence that its result accepts every locale leaf.
///          The conservative semantic type walk checks both condition alternatives, which is required because a result
///          object must remain constructible for either run-time branch.
template <typename char_type, typename output, typename T>
concept lc_bind_one_well_formed_for_output =
	::std::integral<char_type> &&
	requires(::fast_io::basic_lc_all<char_type> const *lc, T &value) {
		::fast_io::details::decay::lc_bind_one<char_type, ::std::remove_reference_t<output>>(
			lc, value);
	};

/// @brief Proves the exact locale-bound operation after every mandatory output-mutex unwrap.
/// @details Runtime acquires each wrapper lock and recurses to its named unlocked observer before it asks whether a
///          locale leaf has an output-specific CPO. The proof follows that same type chain, preserving const at every
///          edge. At the terminal it forms each `lc_bind_one` result from a named source lvalue and removes only the
///          transport reference; this retains a const leaf which ordinary dispatch cannot legally mutate.
template <bool line, ::std::integral char_type, typename output, typename... Args>
inline consteval bool lc_status_print_output_run_okay_impl() noexcept
{
	using normalized_output = ::std::remove_reference_t<output>;
	if constexpr (
		::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<
			normalized_output>)
	{
		if constexpr (
			::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
				normalized_output>)
		{
			using unlocked_output = ::std::remove_reference_t<decltype(
				::fast_io::operations::decay::output_stream_unlocked_ref_decay(
					::std::declval<normalized_output &>()))>;
			return ::fast_io::details::decay::lc_status_print_output_run_okay_impl<
				line, char_type, unlocked_output, Args...>();
		}
		else
		{
			return false;
		}
	}
	else if constexpr (
		(::fast_io::details::decay::lc_bind_one_well_formed_for_output<
			 char_type, normalized_output, Args> && ...))
	{
		return ::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
			line, normalized_output,
			::std::remove_reference_t<decltype(
				::fast_io::details::decay::lc_bind_one<char_type, normalized_output>(
					::std::declval<::fast_io::basic_lc_all<char_type> const *>(),
					::std::declval<Args &>()))>...>;
	}
	else
	{
		return false;
	}
}

template <bool line, ::std::integral char_type, typename output, typename... Args>
inline constexpr bool lc_status_print_output_run_okay{
	::fast_io::details::decay::lc_status_print_output_run_okay_impl<
		line, char_type, output, Args...>()};

template <bool line, ::std::integral char_type, typename output>
struct lc_bound_emit_continuation
{
	::fast_io::basic_lc_all<char_type> const *lc;
	output *out;

	template <typename... Args>
	inline constexpr decltype(auto) operator()(Args &&...args) const
	{
		// Every continuation parameter is named here. Probing and binding these lvalues mirrors the expressions used
		// by reserve/scatter/direct dispatch and avoids inventing an rvalue route for a stored semantic child. The
		// enclosing locale operation already owns or borrows the observer, so continuation storage is one pointer. The
		// final entry borrows both unchanged leaves and temporary pointer bridges synchronously: this preserves a move-only
		// ordinary owner without letting a bridge escape its full expression.
		return ::fast_io::operations::decay::print_freestanding_decay_borrowed_output_and_arguments<line>(
			*out, ::fast_io::details::decay::lc_bind_one<char_type, output>(lc, args)...);
	}
};

template <::std::integral char_type, typename continuation>
struct lc_select_conditions_continuation
{
	::std::remove_reference_t<continuation> *continuation_ptr;

	template <typename... Args>
	inline constexpr decltype(auto) operator()(Args &&...args) const
	{
		return ::fast_io::details::decay::print_semantic_select_conditions<char_type>(
			*continuation_ptr, ::std::forward<Args>(args)...);
	}
};

} // namespace details::decay

template <::std::integral char_type, typename output, typename T>
inline constexpr auto status_io_print_forward(
	::fast_io::io_alias_type_t<char_type>,
	::fast_io::details::decay::lc_deferred_locale_forward<char_type, output, T> deferred)
{
	// Mirror ordinary nested-pack forwarding exactly once, while the source object in the enclosing semantic owner is
	// still alive. The returned bridge then owns a prvalue proxy or points at a stable lvalue proxy; in either case the
	// locale pointer accompanies the result into the ordinary width/condition strategy that requested it.
	decltype(auto) forwarded{
		::fast_io::details::decay::print_semantic_input_forward<char_type>(*deferred.value)};
	return ::fast_io::details::decay::lc_own_forwarded_locale_leaf<char_type>(
		deferred.lc, ::std::move(forwarded));
}

namespace operations::decay
{

/// @brief Emits a locale-aware run through the maintained ordinary strategy engine.
/// @details Mutex ownership is established before output-specific capability selection. Semantic packs are flattened
///          and inactive condition branches are removed before locale binding, after which each locale leaf becomes an
///          ordinary reserve/scatter/direct bridge. This single strategy path is the evidence for keeping locale
///          protocol recognition separate from storage and syscall policy: changes to buffering, coalescing, stack
///          limits, and descriptor batching now apply to locale output automatically.
template <bool line, typename output, typename... Args>
inline constexpr void lc_status_print_define_decay(
	::fast_io::basic_lc_all<typename output::output_char_type> const *lc, output &out, Args &...args)
{
	if constexpr (!line && sizeof...(Args) == 0u)
	{
		// A source-free non-line record has no locale-dependent work. Resolve the print-level empty-record contract
		// before forming the locale selection graph or acquiring a destination mutex: the shared dispatcher ignores
		// an unobservable destination and follows a complete mutex protocol exactly once when the effective output
		// supplies `status_print_define<false>()`. Keeping the remaining graph in a discarded branch is part of the
		// proof, because neither locale CPO discovery nor synchronization may be instantiated for an ignored record.
		return ::fast_io::operations::decay::print_freestanding_empty_run(out);
	}
	else if constexpr (
		::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<output>)
	{
		// Locale binding does not weaken the stream synchronization protocol. In particular, a mutex marker alone
		// cannot justify either constructing the guard or recurring on an unlocked object. The shared complete concept
		// proves exact lock/unlock effects, storable proxies, character preservation, and strict type progress before
		// either expression is instantiated. Reusing the ordinary-print proof here also prevents the two dispatchers
		// from drifting as new output-wrapper concepts are added.
		static_assert(
			::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<output>,
			"locale output requires a complete, character-preserving, type-progressing mutex protocol");
		if constexpr (
			::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<output>)
		{
			// The surrounding `lc_imbuer::status_print_define` already owns the complete source record. An underlying
			// pre-binding `status_print_define<line>(output, Args...)` is not an alternative locale execution branch: active
			// leaves are locale-bound first and only the rebound record enters ordinary IO status selection. Consequently
			// this structural zero-leaf proof needs no core-style exclusion for an underlying source-graph status owner.
			constexpr bool structural_graph_can_select_empty{
				(false || ... ||
				 (::fast_io::details::decay::print_semantic_pack_argument_v<Args> ||
				  ::fast_io::details::decay::print_semantic_top_level_condition_v<Args>))};
			if constexpr (!line && structural_graph_can_select_empty)
			{
				// Locale binding has no leaf to translate when the selected fast_io-owned semantic graph is empty. Resolve
				// the derived zero-argument record through the shared print-level contract before synchronization. The
				// conservative structural proof invokes no provider CPO; an ordinary or width leaf remains inconclusive and
				// therefore preserves the established lock-before-locale-forwarding order.
				if (::fast_io::details::decay::print_semantic_run_provably_empty(args...))
				{
					return ::fast_io::operations::decay::print_freestanding_empty_run(out);
				}
			}
			::fast_io::operations::decay::stream_ref_decay_lock_guard guard{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(out)};
			// Preserve a stable lvalue result or materialize a prvalue unlocked observer exactly once, then borrow that
			// named object through the remainder of locale selection just like the ordinary print dispatcher.
			decltype(auto) unlocked_output =
				::fast_io::operations::decay::output_stream_unlocked_ref_decay(out);
			return ::fast_io::operations::decay::lc_status_print_define_decay<line>(
				lc, unlocked_output, args...);
		}
	}
	else
	{
		using char_type = typename output::output_char_type;
		::fast_io::details::decay::lc_bound_emit_continuation<line, char_type, output> emit{
			lc, __builtin_addressof(out)};
		::fast_io::details::decay::lc_select_conditions_continuation<char_type, decltype(emit)> select{
			__builtin_addressof(emit)};
		return ::fast_io::details::decay::print_semantic_pack_expand<true, char_type>(select, args...);
	}
}

} // namespace operations::decay

template <bool line, typename output, typename... Args>
	requires(
		(!line && sizeof...(Args) == 0u &&
		 ::fast_io::operations::decay::defines::empty_print_observable<
			 ::std::remove_reference_t<output>>) ||
		((line || sizeof...(Args) != 0u) &&
		 ::fast_io::details::decay::lc_status_print_output_run_okay<
			 line,
			 typename ::std::remove_reference_t<output>::output_char_type,
			 output, Args...>))
inline constexpr void status_print_define(::fast_io::lc_imbuer<output> &imb, Args &...args)
{
	// The `lc_imbuer` wrapper and normalized argument owners belong to the enclosing ordinary print operation. Borrowing
	// all of them keeps a reference handle exact, avoids copying a value handle, and leaves the locale plus every
	// borrowed scatter alive until this synchronous status customization returns. A non-line zero-source record has no
	// locale work of its own, so this forwarding CPO may exist only when the normalized handle's effective output already
	// proves empty-record observability. Otherwise merely adding a locale wrapper would manufacture that capability and
	// could acquire an underlying mutex for a record which the unwrapped destination must ignore. Line mode retains this
	// overload because either an exact line-status operation or the required newline remains observable.
	::fast_io::operations::decay::lc_status_print_define_decay<line>(
		__builtin_addressof(imb.locale->all), imb.handle, args...);
}

} // namespace fast_io
