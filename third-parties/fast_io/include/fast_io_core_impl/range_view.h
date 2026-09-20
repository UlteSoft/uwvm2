#pragma once

#include <utility>

#include "operations/printimpl/scatter_copy.h"

namespace fast_io
{
/// @brief A range print view with a known element count.
/// @details The iterator reference, its alias result, and the character-forwarded value answer three different
///          questions and must remain distinct. `source_reference` describes whether dereference names persistent range
///          storage or creates a temporary proxy. `alias_type` identifies the object whose ordinary print alias is
///          forwarded. `forwarded_value_type` alone determines formatting capabilities. Conflating any two of these
///          types can turn a scatter-shaped value into a false proof that its pointed-to characters outlive iteration.
template <::std::integral ch_type, ::std::input_iterator It>
struct sized_range_view_t
{
	using char_type = ch_type;
	using iterator = It;
	using source_reference = ::std::iter_reference_t<iterator>;
	using source_type = ::std::remove_cvref_t<source_reference>;
	using alias_type = ::std::remove_cvref_t<decltype(
		::fast_io::io_print_alias(*::std::declval<iterator &>()))>;
	using forwarded_expression_type = decltype(
		::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(*::std::declval<iterator &>())));
	using forwarded_value_type = ::std::remove_cvref_t<forwarded_expression_type>;
	basic_io_scatter_t<char_type> sep;
	iterator begin;
	::std::size_t size;
};

/// @brief Proves both scatter expressions used by sized-range two-pass materialization.
/// @details Sizing first stores the forwarded result in a `decltype(auto)` local and calls the CPO through that named
///          object, hence `forwarded_expression_type&` after reference collapsing. Emission passes a fresh forwarded
///          result into an `auto&&` lambda and forwards it again, hence the second expression below. These categories
///          can differ for a value-producing alias. Requiring only the public `T&&` compatibility concept would admit
///          an rvalue-only sizing failure or select a sized strategy whose emission overload does not exist. This is
///          deliberately only an expression-shape proof; it says neither that a returned pointer remains live nor that
///          observing the source twice produces the same descriptor.
template <::std::integral char_type, typename forwarded_expression_type>
inline constexpr bool range_two_pass_scatter_printable_v =
	::fast_io::scatter_printable_for<char_type, forwarded_expression_type &> &&
	::fast_io::scatter_printable_for<
		char_type,
		decltype(::std::forward<forwarded_expression_type>(
			::std::declval<forwarded_expression_type &>()))>;

/// @brief Proves that retaining one range element's forwarded scatter cannot outlive its character storage.
/// @details Iterator stability proves only that `*it` continues to name the same source object. It says nothing about
///          storage used internally by that object's alias customization: an lvalue alias is permitted to return a
///          scatter into scratch space which the next call overwrites. Consequently neither an lvalue reference nor a
///          post-alias `basic_io_scatter_t` is a lifetime proof. Admission requires both an lvalue source and an
///          explicit opt-in by that original source type. Requiring the lvalue is deliberately conservative: even a
///          normally stable owning type is unsafe when produced as a transform-view prvalue and destroyed after the
///          alias expression. Producers whose prvalues point into independent storage still use the safe contiguous
///          fallback unless a future, stronger lifetime protocol represents that distinct guarantee.
template <::std::integral char_type, ::std::input_iterator It>
inline constexpr bool sized_range_view_borrowed_scatter_source_v =
	::std::is_lvalue_reference_v<typename sized_range_view_t<char_type, It>::source_reference> &&
	::fast_io::borrowed_scatter_source<
		char_type, typename sized_range_view_t<char_type, It>::source_type>;

/// @brief Admits scatter elements to a sized range only when both passes are semantically repeatable.
/// @details A forward iterator proves that the same element can be visited again; it does not prove that the element's
///          scatter CPO is an observation rather than a consuming/stateful operation. Sized-range reserve and precise
///          protocols first sum descriptor lengths and then call the formatter again to copy characters. If those calls
///          disagree, the measured capacity is no longer a bound for the write. The source-side borrowed marker supplies
///          both missing facts: retained character lifetime and repeatable bytes/length for one logical print. Keeping
///          this conjunction separate from the cv/ref expression proof makes it impossible for a scatter-shaped result
///          or `forward_range` alone to manufacture provenance.
template <::std::integral char_type, ::std::input_iterator It>
inline constexpr bool sized_range_view_two_pass_scatter_element_v =
	::fast_io::range_two_pass_scatter_printable_v<
		char_type, typename sized_range_view_t<char_type, It>::forwarded_expression_type> &&
	::fast_io::sized_range_view_borrowed_scatter_source_v<char_type, It>;

/// @brief Whether every forwarded element has an exact printed length.
/// @details Static-precise and object-precise reserve protocols state exact lengths by contract. A scatter's `len` is
///          exact by construction. Ordinary reserve printing is intentionally excluded because its size is only an
///          upper bound; reusing that bound as a precise length would leave gaps or expose uninitialized characters.
template <::std::integral char_type, ::std::input_iterator It>
inline constexpr bool sized_range_view_precise_element_v =
	::fast_io::static_precise_reserve_printable<
		char_type, typename sized_range_view_t<char_type, It>::forwarded_value_type> ||
	::fast_io::precise_reserve_printable<
		char_type, typename sized_range_view_t<char_type, It>::forwarded_value_type> ||
	::fast_io::sized_range_view_two_pass_scatter_element_v<char_type, It>;

/// @brief Proves that a sized range has one valid contiguous-materialization protocol for each element.
/// @details A fixed reserve bound needs no measuring traversal and therefore works even for an input iterator.
///          Object-dependent reserve and scatter sizes require a forward iterator because sizing consumes one pass and
///          emission consumes another. A scatter additionally requires the source's borrowed/repeatable marker; iterator
///          multipass alone cannot prove that two CPO observations agree. Forward iteration is not itself a formatting
///          capability: combining it with an unprintable element must keep the view concept-negative instead of deferring
///          failure to a body static_assert.
template <::std::integral char_type, ::std::input_iterator It>
inline constexpr bool sized_range_view_reserve_element_v =
	::fast_io::reserve_printable<
		char_type, typename sized_range_view_t<char_type, It>::forwarded_value_type> ||
	(::std::forward_iterator<It> &&
		 (::fast_io::dynamic_reserve_printable<
			  char_type, typename sized_range_view_t<char_type, It>::forwarded_value_type> ||
		  ::fast_io::sized_range_view_two_pass_scatter_element_v<char_type, It>));

template <::std::integral char_type, ::std::input_iterator I>
sized_range_view_t(basic_io_scatter_t<char_type>, I, ::std::size_t) -> sized_range_view_t<char_type, I>;

template <typename scatter_type, ::std::input_iterator I>
	requires requires(scatter_type scatter) {
		typename ::std::remove_cvref_t<scatter_type>::value_type;
		static_cast<basic_io_scatter_t<typename ::std::remove_cvref_t<scatter_type>::value_type>>(scatter);
	}
sized_range_view_t(scatter_type, I, ::std::size_t)
	-> sized_range_view_t<typename ::std::remove_cvref_t<scatter_type>::value_type, I>;

/// @brief A sentinel-terminated, single-pass range print view.
/// @details This representation deliberately avoids premeasurement. It is the correctness fallback for input ranges
///          whose elements have object-dependent lengths: consuming such an iterator to size the output would make a
///          second materialization pass invalid. Iterator and sentinel types remain distinct because `range` does not
///          imply `common_range`; forcing both endpoints into the iterator type either rejects a valid range in an auto
///          return body or requires an extra counted/sized wrapper that changes the one-pass strategy.
template <::std::integral ch_type, ::std::input_iterator It, typename Sentinel = It>
	requires ::std::sentinel_for<Sentinel, It>
struct range_view_t
{
	using char_type = ch_type;
	using iterator = It;
	using sentinel = Sentinel;
	basic_io_scatter_t<char_type> sep;
	iterator begin;
	sentinel end;
};

template <::std::integral char_type, ::std::input_iterator I, typename Sentinel>
	requires ::std::sentinel_for<Sentinel, I>
range_view_t(basic_io_scatter_t<char_type>, I, Sentinel) -> range_view_t<char_type, I, Sentinel>;

template <typename scatter_type, ::std::input_iterator I, typename Sentinel>
	requires ::std::sentinel_for<Sentinel, I> && requires(scatter_type scatter) {
		typename ::std::remove_cvref_t<scatter_type>::value_type;
		static_cast<basic_io_scatter_t<typename ::std::remove_cvref_t<scatter_type>::value_type>>(scatter);
	}
range_view_t(scatter_type, I, Sentinel)
	-> range_view_t<typename ::std::remove_cvref_t<scatter_type>::value_type, I, Sentinel>;

/// @brief Owns a non-borrowed rvalue range until its deferred range-print operation completes.
/// @details `range_view_t` and `sized_range_view_t` deliberately remain iterator-only views for lvalues and standard
///          borrowed ranges. A non-borrowed rvalue cannot use those representations because its iterators may refer to
///          the range object itself (notably a transform view's callable). This wrapper stores the moved range and no
///          iterators. Its print alias obtains fresh iterators only after the wrapper has reached its final location in
///          the enclosing print call, so moving either the source range or this wrapper never leaves cached iterators
///          referring to the pre-move object. The separator retains the same non-owning scatter contract as ordinary
///          `rgvw`; the owning distinction here concerns the range argument only.
template <::std::ranges::range range_type, ::std::integral ch_type>
struct owning_range_view_t
{
	using char_type = ch_type;
	range_type range;
	basic_io_scatter_t<char_type> sep;
};

/// @brief Returns a safe contiguous-capacity bound for a sized range.
/// @details The separator contribution is `(size - 1) * sep.len`. Fixed-reserve elements need no sizing traversal, so
///          a single-pass input iterator remains valid. Object-dependent reserve or scatter elements must be inspected;
///          those paths require a forward iterator because materialization subsequently traverses the same sequence. A
///          scatter path also carries the explicit repeatability proof above, so the later descriptor cannot exceed or
///          otherwise disagree with the length included here.
///          Every multiplication and addition is checked, proving that a successful result is representable in
///          `size_t`. The result is still a reserve capacity, not necessarily an exact output length.
template <::std::integral char_type, ::std::input_iterator It>
	requires(::fast_io::sized_range_view_reserve_element_v<char_type, It>)
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<char_type, sized_range_view_t<char_type, It>>,
												  sized_range_view_t<char_type, It> t)
{
	if (t.size == 0)
	{
		return 0;
	}
	using value_type = typename sized_range_view_t<char_type, It>::forwarded_value_type;
	::std::size_t retval{::fast_io::details::intrinsics::mul_or_overflow_die(t.sep.len, t.size - 1u)};
	if constexpr (reserve_printable<char_type, value_type>)
	{
		return ::fast_io::details::intrinsics::add_or_overflow_die(
			retval,
			::fast_io::details::intrinsics::mul_or_overflow_die(
				print_reserve_size(io_reserve_type<char_type, value_type>), t.size));
	}
	else
	{
		auto curr_ptr{t.begin};
		for (::std::size_t i{}; i != t.size; ++i, ++curr_ptr)
		{
			decltype(auto) value{
				::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(*curr_ptr))};
			::std::size_t element_size;
			if constexpr (dynamic_reserve_printable<char_type, value_type>)
			{
				element_size = print_reserve_size(io_reserve_type<char_type, value_type>, value);
			}
			else
			{
				// `value` is named even when its initializer was a prvalue; test the same lvalue expression passed below.
				static_assert(::fast_io::scatter_printable_for<char_type, decltype((value))>);
				element_size = print_scatter_define(io_reserve_type<char_type, value_type>, value).len;
			}
			retval = ::fast_io::details::intrinsics::add_or_overflow_die(retval, element_size);
		}
		return retval;
	}
}

template <::std::integral char_type, ::std::input_iterator It>
inline constexpr ::std::size_t
print_reserve_static_stack_size(io_reserve_type_t<char_type, sized_range_view_t<char_type, It>>) noexcept
{
	// This hint is consulted only after destination-aware dispatch has rejected both the paired one-pass policy below
	// and the native run-time scatter plan. It therefore sizes scratch for a sink which has already chosen contiguous
	// materialization; it must never serve as the strategy proof itself. Four KiB preserves the measured allocation-free
	// tier for those sinks, while a cheap fake/in-memory boundary no longer instantiates or probes this frame merely
	// because the source is a range. The central print policy still caps the hint for constrained targets. Division
	// converts bytes to characters, and the lower bound preserves a meaningful value for unusually wide character types.
	constexpr ::std::size_t preferred_bytes{4u * 1024u};
	return (preferred_bytes / sizeof(char_type)) == 0u ? 1u : (preferred_bytes / sizeof(char_type));
}

/// @brief Proves every potentially throwing expression in sized-range contiguous emission.
/// @details The public reserve concepts prove capacity and cursor shape, but deliberately say nothing about exception
///          behavior. An overwrite callback needs a stronger fact. The proof below mirrors the ordinary writer body:
///          its local current-iterator copy, dereference and preincrement; the combined alias/character-forward
///          expression; and either the selected reserve writer or the named-and-reforwarded scatter writer. Runtime
///          scatter copies and separator copies use `small_scatter_copy_n`, whose contract is unconditionally
///          `noexcept`.
///
///          This body also sits behind the precise adapter and its delegated ordinary-writer call, both of which pass
///          the view by value. Those copies are intentionally not hidden in this predicate: the nested conditional
///          `noexcept` specifications test each complete call expression, so construction of every parameter is checked
///          at the boundary where it occurs. A value-producing forwarding expression additionally creates a local
///          object in the emitter lambda; its destructor must therefore be non-throwing. Reference results create no
///          such owned object.
template <::std::integral char_type, ::std::input_iterator It>
[[nodiscard]] inline consteval bool
sized_range_view_nothrow_reserve_define() noexcept
{
	using view_type = ::fast_io::sized_range_view_t<char_type, It>;
	using forwarded_expression_type = typename view_type::forwarded_expression_type;
	using forwarded_value_type = typename view_type::forwarded_value_type;
	constexpr bool traditional_iterator_copies_nothrow{
		::std::is_nothrow_constructible_v<It, It const &> &&
		::std::is_nothrow_constructible_v<It, It &>};
#if defined(__HERBCEPTIONS__)
	// The caller copies a const view into the writer's by-value parameter, whereas `auto curr_ptr{t.begin}` copies the
	// member from that named mutable parameter. Both exact constructions execute before the deferred cursor is published.
	constexpr bool deterministic_iterator_copy_throws{
		::std::is_herbceptions_throws_constructible_v<It, It const &> ||
		::std::is_herbceptions_throws_constructible_v<It, It &>};
#else
	constexpr bool deterministic_iterator_copy_throws{};
#endif
	if constexpr (!::fast_io::sized_range_view_reserve_element_v<char_type, It> ||
				  !traditional_iterator_copies_nothrow ||
				  deterministic_iterator_copy_throws ||
				  !::std::is_nothrow_destructible_v<It>)
	{
		return false;
	}
	else if constexpr (
		!::std::is_reference_v<forwarded_expression_type> &&
		(!::std::is_object_v<forwarded_expression_type> ||
		 !requires { sizeof(forwarded_expression_type); }))
	{
		return false;
	}
	else if constexpr (
		!::std::is_reference_v<forwarded_expression_type> &&
		!::std::is_nothrow_destructible_v<forwarded_expression_type>)
	{
		return false;
	}
	else if constexpr (!requires(It &current) {
						   { *current } noexcept;
						   { ++current } noexcept -> ::std::same_as<It &>;
						   {
							   ::fast_io::io_print_forward<char_type>(
								   ::fast_io::io_print_alias(*current))
						   } noexcept;
					   })
	{
		return false;
	}
	else if constexpr (
		::fast_io::reserve_printable<char_type, forwarded_value_type> ||
		::fast_io::dynamic_reserve_printable<char_type, forwarded_value_type>)
	{
		return requires(char_type *ptr, forwarded_expression_type &value) {
			{
				print_reserve_define(
					::fast_io::io_reserve_type<char_type, forwarded_value_type>, ptr,
					::std::forward<forwarded_expression_type>(value))
			} noexcept -> ::std::same_as<char_type *>;
		};
	}
	else
	{
		return requires(forwarded_expression_type &value) {
			{
				print_scatter_define(
					::fast_io::io_reserve_type<char_type, forwarded_value_type>,
					::std::forward<forwarded_expression_type>(value))
			} noexcept -> ::std::same_as<::fast_io::basic_io_scatter_t<char_type>>;
		};
	}
}

template <::std::integral char_type, ::std::input_iterator It>
inline constexpr bool sized_range_view_nothrow_reserve_define_v =
	::fast_io::sized_range_view_nothrow_reserve_define<char_type, It>();

/*
Contiguous staged-range extension
=================================

A scalar range is the ordered byte fold

    emit(x[0]); for i > 0: emit(separator); emit(x[i]).

Some element formatters can prove and implement a stronger, source-aware batch
schedule.  The extension point below deliberately passes the raw contiguous
source, not one generic print proxy per lane: float_DA's gain depends on keeping
classification, cached-power preparation, sign extraction, and ASCII emission
inside one compiler-visible loop.  Reconstructing those operations through the
generic staged CPO for every lane was measured to lose about 1 ns/value on M4.

The extension's semantic contract is strict.  It may reorder only independent
preparation; it must emit the same element spellings and separator positions as
the scalar fold and return their logical end.  The reserve and put-area callers
below separately prove enough physical storage for every scratch store.
*/
/*
The zero-argument declaration is an ADL anchor only.  It is never viable for
the five-argument customization expression.  Naming the extension at template
definition time is required by GCC's two-phase lookup; the source-specific
overload may then be declared later in the floating header and found through
the fast_io tag's associated namespace at instantiation.
*/
void print_contiguous_staged_range_define();
void print_contiguous_staged_range_width();

template <::std::integral char_type, ::std::input_iterator It>
[[nodiscard]] inline consteval bool
sized_range_view_contiguous_staged() noexcept
{
	/*
	Keep this capability query as an immediate function rather than an inline
	variable-template initializer.  Clang 17 otherwise crashes while completing
	the nested requires-expression (InstantiateVariableInitializer from an
	ExprRequirement).  The function has the same compile-time Boolean value and
	every use remains an atomic constraint, while avoiding that frontend path.
	*/
	using view_type = ::fast_io::sized_range_view_t<char_type, It>;
	using value_type = typename view_type::forwarded_value_type;
	if constexpr (
		!::std::same_as<char_type, char> ||
		!::fast_io::details::is_ascii<char_type> ||
		!::std::is_pointer_v<It> ||
		!::fast_io::reserve_printable<char_type, value_type>)
	{
		return false;
	}
	else
	{
		return requires(
			char_type *destination, It source, ::std::size_t count,
			::fast_io::basic_io_scatter_t<char_type> separator)
		{
			/*
			The source-specific kernel owns its profitable scheduling width.
			That width cannot be borrowed from an element's ordinary staged CPO:
			a compiler may normalize the element tag to a representation-
			equivalent extended spelling (for example GCC's `_Float64`) while
			the raw range still stores `double`.  The batch CPO proves that pair;
			an unrelated exact-type policy does not.
			*/
			{
				print_contiguous_staged_range_width(
					::fast_io::io_reserve_type<char_type, value_type>)
			} noexcept -> ::std::same_as<::std::size_t>;
			{
				print_contiguous_staged_range_define(
					::fast_io::io_reserve_type<char_type, value_type>,
					destination, source, count, separator)
			} noexcept -> ::std::same_as<char_type *>;
		};
	}
}

template <::std::integral char_type, ::std::input_iterator It>
	requires(::fast_io::sized_range_view_contiguous_staged<char_type, It>())
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
sized_range_view_staged_define(
	char_type *destination, sized_range_view_t<char_type, It> value) noexcept
{
	using value_type =
		typename ::fast_io::sized_range_view_t<
			char_type, It>::forwarded_value_type;
	return print_contiguous_staged_range_define(
		::fast_io::io_reserve_type<char_type, value_type>, destination,
		value.begin, value.size, value.sep);
}

/// @brief Materializes a sized range into contiguous storage and returns the actual end.
/// @details This is the define half of the ordinary reserve protocol. Each element is forwarded through the same
///          alias pipeline used during sizing. Scatter elements are copied because a contiguous destination cannot
///          retain descriptor boundaries. The function performs one traversal; any preceding object-dependent sizing
///          traversal is the reason such cases require a forward iterator.
template <::std::integral char_type, ::std::input_iterator It>
	requires(::fast_io::sized_range_view_reserve_element_v<char_type, It>)
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, sized_range_view_t<char_type, It>>,
											 char_type *__restrict ptr, sized_range_view_t<char_type, It> t)
#if __cpp_lib_string_resize_and_overwrite >= 202110L
	noexcept(::fast_io::sized_range_view_nothrow_reserve_define<char_type, It>())
#endif
{
	if (t.size == 0)
	{
		return ptr;
	}
	if constexpr (
		::fast_io::sized_range_view_contiguous_staged<char_type, It>())
	{
		/*
		The specialization preserves this function's reserve contract.  For
		element i, the current cursor is no farther than the sum of the preceding
		element reserve bounds plus their separators.  Hence the remaining
		allocation contains at least the complete reserve bound of element i,
		including any fixed-width SIMD scratch store.  Writing a separator at the
		logical end merely overwrites scratch bytes and cannot exceed the global
		range bound.
		*/
		if (!::std::is_constant_evaluated())
		{
			return ::fast_io::sized_range_view_staged_define(ptr, t);
		}
		/*
		The architecture-specific batch leaf is a run-time scheduling
		optimization.  Constant evaluation retains the scalar constexpr fold,
		which computes the same carrier and character sequence.
		*/
	}
	auto curr_ptr{t.begin};
	auto emit_element = [&ptr](auto &&value) constexpr
	{
		using element_type = ::std::remove_cvref_t<decltype(value)>;
		if constexpr (reserve_printable<char_type, element_type> ||
					  dynamic_reserve_printable<char_type, element_type>)
		{
			ptr = print_reserve_define(io_reserve_type<char_type, element_type>, ptr,
									   ::std::forward<decltype(value)>(value));
		}
		else
		{
			// The lambda deliberately preserves the forwarded result's original category for the emitting CPO.
			static_assert(::fast_io::scatter_printable_for<
				char_type, decltype(::std::forward<decltype(value)>(value))>);
			auto scatter{print_scatter_define(io_reserve_type<char_type, element_type>,
										 ::std::forward<decltype(value)>(value))};
			ptr = ::fast_io::details::decay::small_scatter_copy_n(scatter.base, scatter.len, ptr);
		}
	};
	emit_element(::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(*curr_ptr)));
	++curr_ptr;
	for (::std::size_t i{}; i != t.size - 1; ++i, ++curr_ptr)
	{
		ptr = ::fast_io::details::decay::small_scatter_copy_n(t.sep.base, t.sep.len, ptr);
		emit_element(::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(*curr_ptr)));
	}
	return ptr;
}

/// @brief Returns the exact contiguous size of a sized range.
/// @details Only exact element protocols participate. Ordinary reserve bounds are never reused as precise lengths.
///          For statically precise elements, `N * element_size + (N - 1) * separator_size` is exact without traversal.
///          Otherwise each element is measured through its precise protocol (or its scatter length), and the forward-
///          iterator requirement makes the later define pass valid. Checked arithmetic turns representability into an
///          explicit precondition: successful return proves that the exact total fits in `size_t`.
template <::std::integral char_type, ::std::input_iterator It>
	requires(::fast_io::sized_range_view_precise_element_v<char_type, It> &&
			 (::fast_io::static_precise_reserve_printable<
				  char_type, typename sized_range_view_t<char_type, It>::forwarded_value_type> ||
			  ::std::forward_iterator<It>))
inline constexpr ::std::size_t
print_reserve_precise_size(io_reserve_type_t<char_type, sized_range_view_t<char_type, It>>,
						   sized_range_view_t<char_type, It> t)
{
	if (t.size == 0u)
	{
		return 0u;
	}
	using value_type = typename sized_range_view_t<char_type, It>::forwarded_value_type;
	::std::size_t result{
		::fast_io::details::intrinsics::mul_or_overflow_die(t.sep.len, t.size - 1u)};
	if constexpr (::fast_io::static_precise_reserve_printable<char_type, value_type>)
	{
		return ::fast_io::details::intrinsics::add_or_overflow_die(
			result, ::fast_io::details::intrinsics::mul_or_overflow_die(
						print_reserve_static_precise_size(io_reserve_type<char_type, value_type>), t.size));
	}
	else
	{
		auto current{t.begin};
		for (::std::size_t i{}; i != t.size; ++i, ++current)
		{
			decltype(auto) value{
				::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(*current))};
			::std::size_t element_size;
			if constexpr (::fast_io::precise_reserve_printable<char_type, value_type>)
			{
				element_size = print_reserve_precise_size(io_reserve_type<char_type, value_type>, value);
			}
			else
			{
				// Precise sizing observes the same named-lvalue expression as ordinary run-time sizing.
				static_assert(::fast_io::scatter_printable_for<char_type, decltype((value))>);
				element_size = print_scatter_define(io_reserve_type<char_type, value_type>, value).len;
			}
			result = ::fast_io::details::intrinsics::add_or_overflow_die(result, element_size);
		}
		return result;
	}
}

/// @brief Materializes a precisely measured sized range into contiguous storage.
/// @details Exact aggregate length does not upgrade an ordinary element writer to
///          an exact-store writer.  In particular, an ASCII floating formatter may
///          issue a fixed-width SIMD scratch store past its logical result while
///          remaining inside its ordinary reserve bound.  Summing logical element
///          lengths removes that slack, so delegating this function to the ordinary
///          range writer would not prove that the last such store remains inside the
///          destination object.
///
///          The implementation therefore reproduces the range grammar while
///          selecting each element's exact protocol.  A statically precise element
///          has an ordinary writer whose reserve extent equals its logical extent.
///          An object-precise element receives its own measured extent and exact
///          writer.  A scatter copies exactly `len` code units.  Separators are
///          copied only between adjacent elements.  These disjoint writes sum to
///          the already proved aggregate `precise_size`, establishing both the
///          returned endpoint and the absence of a physical store beyond it.
template <::std::integral char_type, ::std::input_iterator It>
	requires(::fast_io::sized_range_view_precise_element_v<char_type, It> &&
			 (::fast_io::static_precise_reserve_printable<
				  char_type, typename sized_range_view_t<char_type, It>::forwarded_value_type> ||
			  ::std::forward_iterator<It>))
inline constexpr char_type *
print_reserve_precise_define(io_reserve_type_t<char_type, sized_range_view_t<char_type, It>>,
							 char_type *__restrict ptr, [[maybe_unused]] ::std::size_t precise_size,
							 sized_range_view_t<char_type, It> t)
{
	using view_type = ::fast_io::sized_range_view_t<char_type, It>;
	using value_type = typename view_type::forwarded_value_type;
	auto current{t.begin};
	for (::std::size_t index{}; index != t.size; ++index, ++current)
	{
		if (index != 0u)
		{
			ptr = ::fast_io::details::decay::small_scatter_copy_n(
				t.sep.base, t.sep.len, ptr);
		}
		decltype(auto) value{
			::fast_io::io_print_forward<char_type>(
				::fast_io::io_print_alias(*current))};
		constexpr auto element_tag{
			::fast_io::io_reserve_type<char_type, value_type>};
		if constexpr (
			::fast_io::static_precise_reserve_printable<char_type, value_type>)
		{
			/*
			Here ordinary reserve size equals the advertised static exact size.
			Consequently its writer has no permission to touch a code unit beyond
			that exact element interval.
			*/
			ptr = print_reserve_define(
				element_tag, ptr,
				::std::forward<decltype(value)>(value));
		}
		else if constexpr (
			::fast_io::precise_reserve_printable<char_type, value_type>)
		{
			auto const element_size{
				print_reserve_precise_size(element_tag, value)};
			using result_type = decltype(print_reserve_precise_define(
				element_tag, ptr, element_size,
				::std::forward<decltype(value)>(value)));
			if constexpr (::std::same_as<result_type, char_type *>)
			{
				ptr = print_reserve_precise_define(
					element_tag, ptr, element_size,
					::std::forward<decltype(value)>(value));
			}
			else
			{
				static_assert(::std::same_as<result_type, void>);
				print_reserve_precise_define(
					element_tag, ptr, element_size,
					::std::forward<decltype(value)>(value));
				ptr += element_size;
			}
		}
		else
		{
			static_assert(
				::fast_io::sized_range_view_two_pass_scatter_element_v<
					char_type, It>);
			auto const scatter{print_scatter_define(
				element_tag, value)};
			ptr = ::fast_io::details::decay::small_scatter_copy_n(
				scatter.base, scatter.len, ptr);
		}
	}
	return ptr;
}

/// @brief Identifies the run-time scatter range as sensitive to destination preinitialization in ordinary concat.
/// @details Exact sizing traverses every element and exact emission traverses it again. If the final string first
///          value-initializes the complete logical range, ordinary concat adds a third full traversal and an extra
///          complete destination write before overwriting those characters. The marker is restricted to the borrowed/
///          repeatable run-time scatter shape; fixed-size ranges and unrelated precise producers retain their
///          established destination policy.
template <::std::integral char_type, ::std::forward_iterator It>
	requires ::fast_io::sized_range_view_two_pass_scatter_element_v<char_type, It>
inline constexpr ::std::true_type print_precise_resize_initialization_sensitive(
	io_reserve_type_t<char_type, sized_range_view_t<char_type, It>>) noexcept
{
	return {};
}

/// @brief Reports a run-time scatter plan for ranges whose forwarded elements are borrowed character scatters.
/// @details The upper bound is one descriptor per element plus one per nonempty separator between adjacent elements.
///          Empty element scatters may make the actual count smaller. Reserve-storage capacity is zero because every
///          descriptor points to existing character storage. Borrow admission is a source-side customization; neither
///          iterator reference category nor forwarded scatter shape can manufacture that proof. The forwarded object
///          need not itself be `basic_io_scatter_t`: an alias/status proxy is equally eligible when the exact named and
///          re-forwarded `print_scatter_define` expressions are valid. Requiring the CPO expression instead of a result-
///          object type keeps protocol admission structural and still obtains the one canonical descriptor type from
///          the CPO. A forward iterator is required because the general dynamic-reserve route may first traverse the
///          range to determine contiguous capacity and because retained references must remain stable.
template <::std::integral char_type, ::std::forward_iterator It>
	requires ::fast_io::sized_range_view_two_pass_scatter_element_v<char_type, It>
inline constexpr reserve_scatters_size_result
print_reserve_scatters_size(io_reserve_type_t<char_type, sized_range_view_t<char_type, It>>,
							sized_range_view_t<char_type, It> t)
{
	if (t.size == 0u)
	{
		return {};
	}
	::std::size_t scatters_size{t.size};
	if (t.sep.len != 0u)
	{
		scatters_size = ::fast_io::details::intrinsics::add_or_overflow_die(scatters_size, t.size - 1u);
	}
	return {scatters_size, 0u};
}

/// @brief Builds a borrowed scatter chain for a sized range.
/// @details Iteration emits each nonempty separator before its following element, exactly matching contiguous range
///          formatting. Empty components emit no descriptor, so the returned descriptor pointer can precede the
///          reported capacity end but can never exceed it: there are at most `N` elements and `N - 1` separators.
///          `reserve_ptr` is returned unchanged because this representation copies no characters. The admission proof
///          above guarantees that every retained element pointer is valid through the final scatter write, including
///          after the iterator has advanced past the element that produced it. Each iteration normalizes the source
///          exactly once, binds that result to a named object, and re-forwards it with its original category. This is
///          the same expression proved by `range_two_pass_scatter_printable_v`; converting a custom proxy directly
///          would bypass its CPO and make the constraint weaker than the implementation.
template <::std::integral char_type, ::std::forward_iterator It>
	requires ::fast_io::sized_range_view_two_pass_scatter_element_v<char_type, It>
inline constexpr basic_reserve_scatters_define_result<char_type>
print_reserve_scatters_define(io_reserve_type_t<char_type, sized_range_view_t<char_type, It>>,
							  basic_io_scatter_t<char_type> *scatters, char_type *reserve_ptr,
							  sized_range_view_t<char_type, It> t)
{
	auto scatter_curr{scatters};
	auto range_curr{t.begin};
	for (::std::size_t i{}; i != t.size; ++i, ++range_curr)
	{
		if (i != 0u && t.sep.len != 0u)
		{
			*scatter_curr++ = t.sep;
		}
		decltype(auto) value{
			::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(*range_curr))};
		using value_type = ::std::remove_cvref_t<decltype(value)>;
		using forwarded_expression_type = decltype(::std::forward<decltype(value)>(value));
		static_assert(::fast_io::scatter_printable_for<char_type, forwarded_expression_type>);
		basic_io_scatter_t<char_type> const element{print_scatter_define(
			::fast_io::io_reserve_type<char_type, value_type>,
			::std::forward<decltype(value)>(value))};
		if (element.len != 0u)
		{
			*scatter_curr++ = element;
		}
	}
	return {scatter_curr, reserve_ptr};
}

template <::std::integral char_type, ::std::forward_iterator It>
	requires ::fast_io::sized_range_view_two_pass_scatter_element_v<char_type, It>
inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	io_reserve_type_t<char_type, sized_range_view_t<char_type, It>>) noexcept
{
	// The dynamic plan above stores only the separator scatter and element scatters already admitted by the original
	// source-side borrowing proof.  Advancing the forward iterator or materializing a neighboring producer cannot
	// overwrite any of those character ranges.
	return {};
}

namespace operations::decay
{
/// @brief Emits unnormalized arguments through an output reference selected by an enclosing print operation.
/// @details `print_define` receives the concrete stream reference after public output normalization.  Calling the
///          public entry again would try to form `output_stream_ref(output_ref)` and can either reject a deliberately
///          non-idempotent observer or add another proxy layer.  The definition of this bridge lives beside the main
///          dispatcher, where it applies the ordinary alias/character-forwarding or semantic-input forwarding exactly
///          once while preserving the caller's source categories. Both this bridge and the range `print_define`
///          overloads borrow the enclosing operation's observer. A range formatter emits synchronously, so copying
///          that observer cannot extend its lifetime; an outer by-value parameter would add one fixed copy and an
///          inner by-value parameter would additionally make observer cost linear in the element count.
template <bool line, typename outputstmtype, typename... Args>
inline constexpr void print_freestanding_decay_unforwarded(outputstmtype &optstm, Args &&...args);
}

/// @brief Proves the source/iterator half of deferred-commit direct scatter streaming.
/// @details `noexcept` and borrowed/repeatable provenance alone do not prove that delaying cursor publication is
///          invisible: a customization may still inspect a destination through shared state. Admission therefore also
///          requires the explicit output-state-independence marker. A second marker proves that the selected scatter is
///          the source's complete element and separator/element print semantics; otherwise an independent status hook
///          could be silently bypassed even though the descriptor bytes themselves were stable. The iterator is
///          restricted to a raw pointer, whose copy, dereference, and increment cannot hide user callbacks; contiguous
///          ranges are already lowered to this representation by `rgvw`. Together with the exact non-throwing
///          expressions below, this proves that the only destination-side operation removed by the strategy is an
///          intermediate cursor publication. The independent stream-side marker must prove that those publications are
///          foldable and that its own output-associated hooks do not alter direct-scatter semantics.
template <::std::integral char_type, ::std::input_iterator It>
[[nodiscard]] inline consteval bool
sized_range_view_nothrow_direct_scatter() noexcept
{
	using view_type = ::fast_io::sized_range_view_t<char_type, It>;
	using forwarded_expression_type = typename view_type::forwarded_expression_type;
	using forwarded_value_type = typename view_type::forwarded_value_type;
	if constexpr (!::std::is_pointer_v<It> || !::std::forward_iterator<It> ||
				  !::fast_io::sized_range_view_two_pass_scatter_element_v<char_type, It> ||
				  !::fast_io::scatter_output_state_independent<
					  char_type, typename view_type::source_type> ||
				  !::fast_io::scatter_direct_print_equivalent<
					  char_type, typename view_type::source_type> ||
				  !::std::is_nothrow_copy_constructible_v<It>)
	{
		return false;
	}
	else
	{
		return requires(It &iterator, forwarded_expression_type &value) {
			{ *iterator } noexcept;
			{ ++iterator } noexcept -> ::std::same_as<It &>;
			{
				::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(*iterator))
			} noexcept;
			{
				print_scatter_define(
					::fast_io::io_reserve_type<char_type, forwarded_value_type>, value)
			} noexcept -> ::std::same_as<::fast_io::basic_io_scatter_t<char_type>>;
			{
				print_scatter_define(
					::fast_io::io_reserve_type<char_type, forwarded_value_type>,
					::std::forward<forwarded_expression_type>(value))
			} noexcept -> ::std::same_as<::fast_io::basic_io_scatter_t<char_type>>;
		};
	}
}

template <::std::integral char_type, ::std::input_iterator It>
inline constexpr bool sized_range_view_nothrow_direct_scatter_v =
	::fast_io::sized_range_view_nothrow_direct_scatter<char_type, It>();

/// @brief Proves the exact, non-throwing put-area operations used by direct scatter streaming.
/// @details The range header precedes the general output-operation concepts in the freestanding include graph, so this
///          local expression proof intentionally mirrors the four cursor CPOs instead of depending on a later header.
///          Exact pointer results make subtraction and the final/prefix commit well-typed; `noexcept` prevents cursor
///          access itself from introducing an exception after bytes have been copied into the proved writable area.
template <typename output, typename char_type>
concept sized_range_view_nothrow_put_area = ::std::integral<char_type> && requires(output &out, char_type *position) {
	{ obuffer_begin(out) } noexcept -> ::std::same_as<char_type *>;
	{ obuffer_curr(out) } noexcept -> ::std::same_as<char_type *>;
	{ obuffer_end(out) } noexcept -> ::std::same_as<char_type *>;
	{ obuffer_set_curr(out, position) } noexcept -> ::std::same_as<void>;
};

/// @brief Streams a proved scatter range directly through the current put area without generic pair dispatch.
/// @details This helper is intentionally kept outside the general range formatter. GCC 15 expanded the shared
///          run-time-length copy policy to roughly 1.5 KiB for `string_view` elements; inlining that body perturbed the
///          short fallback even when the element-count gate rejected it. On the fitting path each source is observed
///          exactly once and its descriptor is copied before the next producer is invoked. Capacity is tested as two
///          ordered subtractions, so neither separator-plus-element overflow nor an out-of-bounds speculative write is
///          possible. A single final commit is permitted only by the independent source and stream markers: ordinary
///          `noexcept`/obuffer concepts are deliberately insufficient for this transformation.
///
///          A short put area does not restart the range. The helper first publishes every physically copied prefix byte,
///          then sends the already-obtained boundary descriptor through the ordinary borrowed-output bridge and lets
///          that bridge emit the remaining elements. Caching that descriptor is essential: calling the original CPO
///          again would add an observation not present in the historical single-pass path. The potentially throwing
///          overflow/write operation therefore begins only after the prefix is logically committed. Although that
///          recovery block increases this specialization's static size, it intentionally remains in the same function:
///          two GCC 15 cold-helper experiments changed fitting-loop register allocation and regressed 16--512 element
///          string sinks by 13--56 percent. Replacing the pair bridge with primitive writes is not a valid size remedy;
///          it can change status dispatch, lock scope, alias lifetime, and the prefix visible on an allocation failure.
	template <::std::integral char_type, ::std::input_iterator It, typename output>
		requires(
			::fast_io::sized_range_view_nothrow_direct_scatter<char_type, It>() &&
		::fast_io::sized_range_view_nothrow_put_area<output, char_type> &&
		::fast_io::deferred_obuffer_commit_safe<char_type, output>)
#if __has_cpp_attribute(__gnu__::__noinline__) && __has_cpp_attribute(__gnu__::__aligned__)
// This specialization is deliberately out of line and its fitting loop is several KiB after optimization. On x86-64,
// GCC 15 otherwise lets COMDAT order move the loop between cache-line phases: two instruction-identical instantiations
// differed by 20--25% when the loop entry landed at byte 61 instead of byte 29 of a 64-byte line. Aligning the function,
// rather than a benchmark caller, makes that internal phase deterministic. It adds at most 63 bytes of padding per
// admitted specialization, changes no call or loop operation, and measured 112 bytes (+0.0025%) for the complete
// benchmark.
[[__gnu__::__noinline__, __gnu__::__aligned__(64)]]
#elif __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr bool try_sized_range_view_put_area_direct_scatter(
	output &out, sized_range_view_t<char_type, It> const &t)
{
	char_type *const initial{obuffer_curr(out)};
	char_type *const end{obuffer_end(out)};
	// A zero-capacity adapter may use null sentinels. Pointer subtraction, including null minus null, is not the
	// operation which proves that representation valid, so leave it on the established streaming fallback.
	if (initial == nullptr || end == nullptr)
	{
		return false;
	}
	::std::ptrdiff_t const initial_available_difference{end - initial};
	if (initial_available_difference < 0)
	{
		return false;
	}
	char_type *cursor{initial};
	auto current{t.begin};
	for (::std::size_t index{}; index != t.size; ++index, ++current)
	{
		decltype(auto) value{
			::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(*current))};
		using value_type = ::std::remove_cvref_t<decltype(value)>;
		::fast_io::basic_io_scatter_t<char_type> const element{print_scatter_define(
			::fast_io::io_reserve_type<char_type, value_type>,
			::std::forward<decltype(value)>(value))};
		::std::ptrdiff_t const available_difference{end - cursor};
		::std::size_t const separator_size{index == 0u ? 0u : t.sep.len};
		if (static_cast<::std::size_t>(available_difference) < separator_size ||
			static_cast<::std::size_t>(available_difference) - separator_size < element.len) [[unlikely]]
		{
			if (cursor != initial)
			{
				// Publish every physically copied byte before the ordinary overflow path can throw or relocate the area.
				obuffer_set_curr(out, cursor);
			}
			if (index == 0u)
			{
				::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(out, element);
			}
			else
			{
				::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
					out, t.sep, element);
			}
			for (++index, ++current; index != t.size; ++index, ++current)
			{
				::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
					out, t.sep, *current);
			}
			return true;
		}
		if (separator_size != 0u)
		{
			cursor = ::fast_io::details::decay::small_scatter_copy_n(
				t.sep.base, separator_size, cursor);
		}
		if (element.len != 0u)
		{
			cursor = ::fast_io::details::decay::small_scatter_copy_n(
				element.base, element.len, cursor);
		}
	}
	if (cursor != initial)
	{
		obuffer_set_curr(out, cursor);
	}
	return true;
}

/// @brief Streams a contiguous staged range through one already-large put area.
/// @details A failed capacity proof performs no write and leaves the ordinary
///          one-pass range path in control.  On success, the static per-element
///          reserve bound R and separator length S give the complete physical
///          bound
///
///              N*R + (N-1)*S.
///
///          The two overflow tests below prove that this integer exists in
///          `size_t`; comparison with the actual put-area extent then proves
///          that every fixed-width SIMD scratch store used by the batch leaf is
///          inside the destination.  Since staged conversion/emission is
///          non-throwing, publishing the final cursor once is observationally
///          equivalent on outputs which explicitly permit deferred commit.
template <::std::integral char_type, ::std::contiguous_iterator It,
		  typename output>
	requires(
		::fast_io::sized_range_view_contiguous_staged<char_type, It>() &&
		::fast_io::sized_range_view_nothrow_put_area<output, char_type> &&
		::fast_io::deferred_obuffer_commit_safe<char_type, output>)
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
try_sized_range_view_put_area_staged(
	output &out, sized_range_view_t<char_type, It> const &value) noexcept
{
	if (value.size == 0u)
	{
		return true;
	}
	using view_type = ::fast_io::sized_range_view_t<char_type, It>;
	using value_type = typename view_type::forwarded_value_type;
	constexpr auto element_tag{
		::fast_io::io_reserve_type<char_type, value_type>};
	constexpr ::std::size_t element_reserve{
		print_reserve_size(element_tag)};
	constexpr ::std::size_t maximum_size{
		(::std::numeric_limits<::std::size_t>::max)()};
	if (value.size > maximum_size / element_reserve)
	{
		/*
		The proposed one-area optimization has no representable physical bound.
		This says nothing about the stream's ability to grow and flush while
		using the scalar path, so rejection must not terminate the operation.
		*/
		return false;
	}
	::std::size_t required{value.size * element_reserve};
	if (value.sep.len != 0u)
	{
		auto const separator_count{value.size - 1u};
		if (separator_count >
			(maximum_size - required) / value.sep.len)
		{
			return false;
		}
		required += separator_count * value.sep.len;
	}

	char_type *const initial{obuffer_curr(out)};
	char_type *const end{obuffer_end(out)};
	if (initial == nullptr || end == nullptr)
	{
		return false;
	}
	auto const available_difference{end - initial};
	if (available_difference < 0 ||
		static_cast<::std::size_t>(available_difference) < required)
	{
		return false;
	}
	auto *const final{
		::fast_io::sized_range_view_staged_define(initial, value)};
	obuffer_set_curr(out, final);
	return true;
}

/// @brief Emits a fixed-reserve sized range into one already-sufficient put area.
/// @details The ordinary one-pass range formatter publishes the output cursor after every element pair. That is the
///          right fallback when the destination must grow or flush, but it is unnecessary for an append destination
///          whose current put area already contains the complete static reserve bound. In that case the range reserve
///          protocol is itself a one-traversal writer, so using it here removes all intermediate CPO redispatch and
///          cursor commits without reintroducing the discarded measurement pass which `print_put_area_preferred`
///          deliberately avoids.
///
///          This is not inferred from the presence of cursor CPOs alone. The element formatter must have a static
///          reserve bound, the complete range writer must be non-throwing, and the destination must explicitly permit
///          deferred cursor publication. A failed overflow/capacity proof performs no writes and leaves the established
///          streaming path in control.
template <::std::integral char_type, ::std::input_iterator It, typename output>
	requires(
		::fast_io::reserve_printable<
			char_type, typename ::fast_io::sized_range_view_t<char_type, It>::forwarded_value_type> &&
		::fast_io::sized_range_view_nothrow_reserve_define<char_type, It>() &&
		::fast_io::sized_range_view_nothrow_put_area<output, char_type> &&
		::fast_io::deferred_obuffer_commit_safe<char_type, output>)
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
try_sized_range_view_put_area_reserved(
	output &out, sized_range_view_t<char_type, It> const &value) noexcept
{
	if (value.size == 0u)
	{
		return true;
	}
	using view_type = ::fast_io::sized_range_view_t<char_type, It>;
	using value_type = typename view_type::forwarded_value_type;
	constexpr auto element_tag{
		::fast_io::io_reserve_type<char_type, value_type>};
	constexpr ::std::size_t element_reserve{
		print_reserve_size(element_tag)};
	constexpr ::std::size_t maximum_size{
		(::std::numeric_limits<::std::size_t>::max)()};
	if constexpr (element_reserve != 0u)
	{
		if (value.size > maximum_size / element_reserve)
		{
			return false;
		}
	}
	::std::size_t required{value.size * element_reserve};
	if (value.sep.len != 0u)
	{
		auto const separator_count{value.size - 1u};
		if (separator_count > (maximum_size - required) / value.sep.len)
		{
			return false;
		}
		required += separator_count * value.sep.len;
	}

	char_type *const initial{obuffer_curr(out)};
	char_type *const end{obuffer_end(out)};
	if (initial == nullptr || end == nullptr)
	{
		return false;
	}
	auto const available_difference{end - initial};
	if (available_difference < 0 ||
		static_cast<::std::size_t>(available_difference) < required)
	{
		return false;
	}
	auto *const final{print_reserve_define(
		::fast_io::io_reserve_type<char_type, view_type>, initial, value)};
	obuffer_set_curr(out, final);
	return true;
}

/// @brief Streams an unsized range in one pass.
/// @details The first element is emitted separately, then every remaining element is preceded by the separator. This
///          construction proves that there is no leading or trailing separator and works for single-pass iterators.
template <::std::integral char_type, ::std::input_iterator It, typename Sentinel, typename output>
	requires ::std::sentinel_for<Sentinel, It>
inline constexpr void print_define(io_reserve_type_t<char_type, range_view_t<char_type, It, Sentinel>>, output &out,
								   range_view_t<char_type, It, Sentinel> t)
{
	if (t.begin == t.end)
	{
		return;
	}
	auto curr_ptr{t.begin};
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(out, *curr_ptr);
	for (++curr_ptr; curr_ptr != t.end; ++curr_ptr)
	{
		::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
			out, t.sep, *curr_ptr);
	}
}

/// @brief Streams a sized range in one pass without premeasurement.
/// @details Buffered and append-oriented destinations can grow amortized storage as characters arrive. On those
///          streams this route avoids a complete sizing traversal and a separate temporary materialization while
///          preserving exactly the same first-element/preceding-separator ordering as the unsized view.
template <::std::integral char_type, ::std::input_iterator It, typename output>
inline constexpr void print_define(io_reserve_type_t<char_type, sized_range_view_t<char_type, It>>, output &out,
								   sized_range_view_t<char_type, It> t)
{
	if (t.size == 0u)
	{
		return;
	}
	if constexpr (
		::fast_io::sized_range_view_contiguous_staged<char_type, It>() &&
		::fast_io::sized_range_view_nothrow_put_area<output, char_type> &&
		::fast_io::deferred_obuffer_commit_safe<char_type, output>)
	{
		/*
		At least one complete measured group is required to amortize the put-area
		proof.  Short ranges retain the scalar path and do not instantiate a
		partial batch schedule at run time.
		*/
		using view_type =
			::fast_io::sized_range_view_t<char_type, It>;
		using value_type = typename view_type::forwarded_value_type;
		constexpr auto element_tag{
			::fast_io::io_reserve_type<char_type, value_type>};
		constexpr ::std::size_t extent{
			print_contiguous_staged_range_width(element_tag)};
		/*
		The customization's width is a scheduling theorem, not input data: it
		must be a positive translation-time constant.  This assertion is placed
		after the signature-only ADL probe deliberately.  GCC 11--14 incorrectly
		try to constant-evaluate the zero-argument lookup anchor when the same
		dependent call appears as an `integral_constant` argument inside a
		requires-expression.  Once this function is instantiated, the complete
		one-argument overload set is known, so `constexpr` proves evaluability
		and the assertion proves positivity without weakening the CPO contract
		or creating a run-time branch.
		*/
		static_assert(extent != 0u);
		if (extent <= t.size &&
			::fast_io::try_sized_range_view_put_area_staged(out, t))
		{
			return;
		}
	}
	if constexpr (
		::fast_io::reserve_printable<
			char_type, typename ::fast_io::sized_range_view_t<char_type, It>::forwarded_value_type> &&
		::fast_io::sized_range_view_nothrow_reserve_define<char_type, It>() &&
		::fast_io::sized_range_view_nothrow_put_area<output, char_type> &&
		::fast_io::deferred_obuffer_commit_safe<char_type, output>)
	{
		if (::fast_io::try_sized_range_view_put_area_reserved(out, t))
		{
			return;
		}
	}
	if constexpr (
		::fast_io::sized_range_view_nothrow_direct_scatter<char_type, It>() &&
		::fast_io::sized_range_view_nothrow_put_area<output, char_type> &&
		::fast_io::deferred_obuffer_commit_safe<char_type, output>)
	{
		// The out-of-line direct loop removes per-element generic dispatch, but its fixed call and cursor-validation cost is
		// not free. With all semantic opt-ins in force, GCC 15's sixteen-element string-like specializations already paid
		// more for the retained generic fallback than for the direct loop boundary; 128/512-element cases amortized it
		// decisively. One-element cases remain below the gate. Sixteen is an evidence threshold for this exact proved
		// strategy, not a claim that an unmarked producer or destination has the same crossover.
		constexpr ::std::size_t minimum_direct_scatter_count{16u};
		if (minimum_direct_scatter_count <= t.size &&
			::fast_io::try_sized_range_view_put_area_direct_scatter<char_type>(out, t))
		{
			return;
		}
	}
	auto current{t.begin};
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(out, *current);
	++current;
	::std::size_t const remaining{t.size - 1u};
	auto emit_remaining_element = [&out, &t](auto &&element) constexpr
	{
		// Keeping separator and element in one print run lets the mature reserve/scatter scanner coalesce the pair. Two
		// independent write calls were measured to erase the one-pass benefit on long ranges even when both target the
		// same put area.
		::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
			out, t.sep, ::std::forward<decltype(element)>(element));
	};
	if constexpr (
		::std::contiguous_iterator<It> && ::std::integral<::std::iter_difference_t<It>>)
	{
		using difference_type = ::std::iter_difference_t<It>;
		constexpr auto difference_max{(::std::numeric_limits<difference_type>::max)()};
		if (::std::cmp_less_equal(remaining, difference_max))
		{
			// `sized_range_view_t` was formed from a contiguous sized source, so advancing the post-first cursor by
			// `remaining` reaches the same one-past endpoint as incrementing it that many times. The explicit difference
			// bound makes the random-access operation representable. Comparing one cursor with that invariant endpoint
			// removes the independent count induction from the hot loop and gives LLVM/GCC a canonical pointer-end form.
			auto const last{current + static_cast<difference_type>(remaining)};
			for (; current != last; ++current)
			{
				emit_remaining_element(*current);
			}
			return;
		}
	}
	for (::std::size_t i{}; i != remaining; ++i, ++current)
	{
		// Non-contiguous iterators (and a theoretical count outside their difference range) retain the count-based
		// single-pass fallback; no random-access operation is assumed for that representation.
		emit_remaining_element(*current);
	}
}

template <::std::integral char_type, ::std::input_iterator It>
	requires(
		::fast_io::reserve_printable<
			char_type, typename sized_range_view_t<char_type, It>::forwarded_value_type> ||
		(::std::forward_iterator<It> &&
		 ::fast_io::sized_range_view_two_pass_scatter_element_v<char_type, It>))
inline constexpr ::std::true_type
print_put_area_preferred(io_reserve_type_t<char_type, sized_range_view_t<char_type, It>>) noexcept
{
	// Fixed-reserve elements need no discarded sizing traversal. Borrowed/repeatable scatter elements obtain the same
	// cost property from their source proof plus exact CPO-expression proof; requiring a raw descriptor result here
	// would unnecessarily exclude aliases and status proxies. In both cases the preference is intentionally limited to
	// a real put area. A generic append marker is not an equivalent cost proof: a 128-element range was 127% slower
	// through libstdc++'s append adapter and concat was 383% slower when this source inherited the broader buffered
	// preference. Keeping one combined marker also avoids overlapping customization overloads for an element type that
	// supplies both a fixed reserve protocol and a borrowed scatter refinement.
	return {};
}

template <::std::integral char_type, ::std::input_iterator It>
	requires(
		::fast_io::reserve_printable<
			char_type, typename sized_range_view_t<char_type, It>::forwarded_value_type> ||
		(::std::forward_iterator<It> &&
		 ::fast_io::sized_range_view_two_pass_scatter_element_v<char_type, It>))
inline constexpr ::std::true_type
print_one_pass_preferred(io_reserve_type_t<char_type, sized_range_view_t<char_type, It>>) noexcept
{
	// The same source shapes admitted by the true-put-area policy can stream without a discarded sizing traversal.
	// This broader marker is still inert until the exact destination opts into cheap direct streaming. In particular,
	// POSIX files do not inherit it structurally, and their native run-time scatter plan retains first priority.
	return {};
}

namespace manipulators
{
/// @brief Proves the normalization expression used for each stored iterator dereference.
/// @details The loop dereferences a named iterator lvalue. Probing `*begin(r)` instead observes a temporary iterator and
///          can select a different ref-qualified dereference overload. Keeping the exact expression in a concept turns
///          an invalid alias/forward result into substitution-false before an `auto` return body is instantiated.
template <typename char_type, typename range_type>
concept range_element_print_forwardable =
	::std::integral<char_type> && ::std::ranges::range<range_type> &&
	requires(::std::ranges::iterator_t<range_type> &iterator) {
		::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(*iterator));
	};

namespace range_view_details
{

/// @brief Builds the iterator-only implementation after the range lifetime has already been proved.
/// @details Public lvalue/borrowed overloads call this directly. An owning rvalue wrapper calls it later from its print
///          alias, when the stored range is a stable lvalue. Keeping iterator acquisition here is essential: obtaining
///          iterators before moving a non-borrowed range into the wrapper would not be valid for a general C++20 range.
template <::std::ranges::range rg, ::std::integral char_type>
	requires ::fast_io::manipulators::range_element_print_forwardable<char_type, rg>
inline constexpr auto make_nonowning_range_view(
	rg &&r, ::fast_io::basic_io_scatter_t<char_type> printed)
{
	using ::fast_io::range_view_t;
	using ::fast_io::sized_range_view_t;
	using ::std::to_address;
	using ::std::ranges::begin;
	using ::std::ranges::end;
	using ::std::ranges::size;
	using forwarded_expression_type = decltype(::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(
		*::std::declval<::std::ranges::iterator_t<rg> &>())));
	using forwarded_value_type = ::std::remove_cvref_t<forwarded_expression_type>;
	using iterator_type = ::std::ranges::iterator_t<rg>;
	constexpr bool is_contiguous_range = ::std::ranges::contiguous_range<rg>;
	constexpr bool is_sized_range = ::std::ranges::sized_range<rg>;
	constexpr bool is_forward_range = ::std::ranges::forward_range<rg>;
	constexpr bool is_common_range = ::std::ranges::common_range<rg>;
	constexpr bool is_fixed_reserve = ::fast_io::reserve_printable<char_type, forwarded_value_type>;
	constexpr bool is_runtime_sized = ::fast_io::dynamic_reserve_printable<char_type, forwarded_value_type> ||
									  ::fast_io::sized_range_view_two_pass_scatter_element_v<
										  char_type, iterator_type>;
	if constexpr (is_fixed_reserve || (is_runtime_sized && is_forward_range))
	{
		if constexpr (is_contiguous_range && is_sized_range)
		{
			return sized_range_view_t{printed, to_address(begin(r)), size(r)};
		}
		else if constexpr (is_contiguous_range && is_common_range)
		{
			auto const first{to_address(begin(r))};
			return sized_range_view_t{printed, first, to_address(end(r)) - first};
		}
		else if constexpr (is_sized_range)
		{
			return sized_range_view_t{printed, begin(r), size(r)};
		}
		else
		{
			return range_view_t{printed, begin(r), end(r)};
		}
	}
	else
	{
		if constexpr (is_contiguous_range && is_common_range)
		{
			return range_view_t{printed, to_address(begin(r)), to_address(end(r))};
		}
		else
		{
			return range_view_t{printed, begin(r), end(r)};
		}
	}
}

} // namespace range_view_details

/// @brief Creates a range-printing view with a literal separator.
/// @details The terminating null is excluded, so an `N`-element literal contributes `N - 1` separator characters;
///          this supports multi-character separators and preserves embedded nulls before the terminator. A sized view
///          is selected only when its reserve protocol is iterator-safe: fixed per-type bounds need no measuring pass,
///          while object-dependent sizes require a forward range and scatter sizing additionally requires explicit
///          borrowed/repeatable provenance. Otherwise the sentinel view retains one-pass input-range semantics.
///          Contiguous ranges store pointers as an implementation optimization without changing this protocol decision.
template <::std::ranges::range rg, ::std::integral char_type, ::std::size_t n>
	requires(n != 0u && ::std::ranges::borrowed_range<rg> &&
			 ::fast_io::manipulators::range_element_print_forwardable<char_type, rg>)
inline constexpr auto rgvw(rg &&r, char_type const (&sep)[n])
{
	// Array extent includes the terminator. Unlike the former one-character special case, this retains the complete
	// literal payload and makes separator accounting identical to an ordinary character scatter.
	auto const printed = ::fast_io::basic_io_scatter_t<char_type>{
		reinterpret_cast<char_type const *>(__builtin_addressof(sep)), n - 1u};
	return ::fast_io::manipulators::range_view_details::make_nonowning_range_view(
		::std::forward<rg>(r), printed);
}

/// @brief Owns a non-borrowed rvalue range while preserving the historical nested `rgvw` call syntax.
/// @details The separator remains a scatter exactly as in the iterator-only overload. The range itself is moved into
///          `owning_range_view_t`; no iterator is formed here. This makes both immediate nested printing and storing the
///          returned view safe for owning containers and parent-referencing views such as `transform_view`.
template <::std::ranges::range rg, ::std::integral char_type, ::std::size_t n>
	requires(n != 0u && !::std::ranges::borrowed_range<rg> &&
			 !::std::is_lvalue_reference_v<rg> &&
			 ::std::constructible_from<::std::remove_cvref_t<rg>, rg &&> &&
			 ::fast_io::manipulators::range_element_print_forwardable<char_type, rg>)
inline constexpr auto rgvw(rg &&r, char_type const (&sep)[n])
{
	using range_type = ::std::remove_cvref_t<rg>;
	auto const printed = ::fast_io::basic_io_scatter_t<char_type>{
		reinterpret_cast<char_type const *>(__builtin_addressof(sep)), n - 1u};
	return ::fast_io::owning_range_view_t<range_type, char_type>{
		::std::forward<rg>(r), printed};
}

/// @brief Names the result type of applying the print-alias protocol to a string-like separator.
/// @details The probe uses a mutable lvalue of the separator's underlying type because the eventual factory aliases its
///          named parameter exactly once. This hidden alias preserves references returned by the alias CPO until later
///          code explicitly removes transport cv/ref qualifiers.
template <typename T>
using range_separator_alias_result_t = decltype(print_alias_define(
	::fast_io::io_alias, ::std::declval<::std::remove_reference_t<T> &>()));

/// @brief Proves that a string-like separator may be stored as a non-owning scatter in a range view.
/// @details The view stores neither its range nor its separator. Consequently the range must satisfy the standard
///          borrowed-range lifetime rule, and the separator's alias source must explicitly opt into retained-scatter
///          borrowing. An lvalue leaves its source lifetime under the caller's control. An rvalue is admitted only when
///          `borrowed_range<T>` proves that destroying the range object does not invalidate iterators into its payload;
///          this admits a temporary `std::string_view` but rejects a temporary owning `std::string`. The provenance
///          marker remains necessary because borrowed-range status alone cannot rule out an alias backed by reusable
///          scratch. Literal separators use the separate array overload and retain full-expression array lifetime.
template <typename T>
concept stable_range_separator =
	::fast_io::constructible_to_os_c_str<T> &&
	(::std::is_lvalue_reference_v<T &&> || ::std::ranges::borrowed_range<T>) && requires {
		typename ::std::remove_cvref_t<
			::fast_io::manipulators::range_separator_alias_result_t<T>>::value_type;
		requires ::fast_io::borrowed_scatter_source<
			typename ::std::remove_cvref_t<
				::fast_io::manipulators::range_separator_alias_result_t<T>>::value_type,
			::std::remove_cvref_t<T>>;
	};

/// @brief Extracts the separator code-unit type from a stable string-like separator alias.
/// @details This hidden alias removes transport cv/ref qualifiers and is defined only for separators accepted by the
///          preceding alias protocol.
template <typename T>
using range_separator_char_type_t = typename ::std::remove_cvref_t<
	::fast_io::manipulators::range_separator_alias_result_t<T>>::value_type;

/// @brief Creates a non-owning range view with a stable string-like separator.
/// @details The borrowed range and separator source must both outlive formatting. Each range element is normalized and
///          separators are inserted only between elements, never before the first or after the last.
template <::std::ranges::range rg, ::fast_io::manipulators::stable_range_separator T>
	requires(::std::ranges::borrowed_range<rg> &&
			 ::fast_io::manipulators::range_element_print_forwardable<
				 ::fast_io::manipulators::range_separator_char_type_t<T>, rg>)
inline constexpr auto rgvw(rg &&r, T &&sep)
{
	decltype(auto) printed = print_alias_define(io_alias, sep);
	// The admission concept deliberately permits a stable alias reference as well as an owned scatter value. Strip
	// transport cv/ref here for the same reason; querying a member directly on `decltype(printed)` would accept the
	// reference during constraint checking and then fail only inside this function body.
	using char_type = typename ::std::remove_cvref_t<decltype(printed)>::value_type;
	return ::fast_io::manipulators::range_view_details::make_nonowning_range_view(
		::std::forward<rg>(r),
		static_cast<::fast_io::basic_io_scatter_t<char_type>>(printed));
}

/// @brief Creates an owning range view for a non-borrowed rvalue range with a stable string-like separator.
/// @details The range is moved into the result, while the separator remains a proved-stable borrowed scatter. Element
///          ordering and between-elements-only separator insertion match the non-owning overload.
template <::std::ranges::range rg, ::fast_io::manipulators::stable_range_separator T>
	requires(!::std::ranges::borrowed_range<rg> &&
			 !::std::is_lvalue_reference_v<rg> &&
			 ::std::constructible_from<::std::remove_cvref_t<rg>, rg &&> &&
			 ::fast_io::manipulators::range_element_print_forwardable<
				 ::fast_io::manipulators::range_separator_char_type_t<T>, rg>)
inline constexpr auto rgvw(rg &&r, T &&sep)
{
	using range_type = ::std::remove_cvref_t<rg>;
	decltype(auto) printed = print_alias_define(io_alias, sep);
	using char_type = typename ::std::remove_cvref_t<decltype(printed)>::value_type;
	return ::fast_io::owning_range_view_t<range_type, char_type>{
		::std::forward<rg>(r),
		static_cast<::fast_io::basic_io_scatter_t<char_type>>(printed)};
}

} // namespace manipulators

/// @brief Borrows an owned range only after the owner has reached the print operation's stable argument storage.
/// @details Public print entry points normalize named lvalue parameters. The returned iterator-only alias is therefore
///          consumed while `value` remains alive. Direct aliasing of an rvalue does not select this overload and keeps
///          the owning object intact instead of returning iterators which could escape that temporary.
template <::std::ranges::range range_type, ::std::integral char_type>
	requires ::fast_io::manipulators::range_element_print_forwardable<char_type, range_type &>
inline constexpr auto print_alias_define(
	::fast_io::io_alias_t, ::fast_io::owning_range_view_t<range_type, char_type> &value)
{
	return ::fast_io::manipulators::range_view_details::make_nonowning_range_view(
		value.range, value.sep);
}

template <::std::ranges::range range_type, ::std::integral char_type>
	requires(::std::ranges::range<range_type const &> &&
			 ::fast_io::manipulators::range_element_print_forwardable<
				 char_type, range_type const &>)
inline constexpr auto print_alias_define(
	::fast_io::io_alias_t,
	::fast_io::owning_range_view_t<range_type, char_type> const &value)
{
	return ::fast_io::manipulators::range_view_details::make_nonowning_range_view(
		value.range, value.sep);
}

} // namespace fast_io
