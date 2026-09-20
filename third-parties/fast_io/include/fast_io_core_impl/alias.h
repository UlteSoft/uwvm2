#pragma once

namespace fast_io
{

namespace details
{

struct cannot_output_type
{
	inline explicit constexpr cannot_output_type() noexcept = default;
};

/// @brief Recognizes a complete object before any library trait which requires completeness is instantiated.
/// @details `is_object` deliberately accepts forward-declared class types, whereas `sizeof` and several standard
///          triviality/constructibility traits do not. Keeping the completeness probe in a requires-expression makes
///          an incomplete cv/ref object an ordinary false result. Callers must test this concept in an `if constexpr`
///          before forming those traits; spelling all tests in one Boolean expression is not a substitution-safety
///          proof because a variable-template trait may diagnose its incomplete argument while being instantiated.
template <typename T>
concept io_print_complete_object =
	::std::is_object_v<::std::remove_cvref_t<T>> &&
	requires { sizeof(::std::remove_cvref_t<T>); };

/// @brief Computes the exception contract of print aliasing from the selected protocol branch.
/// @details This duplicates only the compile-time branch selection used by `io_print_alias`; it never evaluates the
///          argument.  Keeping the proof beside the dispatcher prevents a throwing ADL customization from being
///          hidden behind an unconditional `noexcept` while retaining useful nothrow information for ordinary values.
template <typename T>
inline constexpr bool io_print_alias_nothrow = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t>)
	{
		return true;
	}
	else if constexpr (::fast_io::alias_printable<T>)
	{
		return noexcept(print_alias_define(::fast_io::io_alias, ::std::declval<T>()));
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else if constexpr (!::fast_io::details::io_print_complete_object<T>)
	{
		return false;
	}
	else
	{
		return ::std::is_nothrow_constructible_v<no_cvref_t, T &&>;
	}
}();

#if defined(__HERBCEPTIONS__)
/// @brief Computes only the deterministic error effect of the branch selected by `io_print_alias`.
/// @details This proof is deliberately independent of `io_print_alias_nothrow`: a plain function may still propagate a
///          traditional C++ exception, whereas a Herbception condition controls the deterministic-result ABI.
template <typename T>
inline constexpr bool io_print_alias_herbceptions_throws = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t>)
	{
		return false;
	}
	else if constexpr (::fast_io::alias_printable<T>)
	{
		return throws((print_alias_define(::fast_io::io_alias, ::std::declval<T>())));
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return false;
	}
	else if constexpr (!::fast_io::details::io_print_complete_object<T>)
	{
		return false;
	}
	else
	{
		return ::std::is_herbceptions_throws_constructible_v<no_cvref_t, T &&>;
	}
}();
#endif

/// @brief Largest object eligible for the normalized print transport's bounded ownership-copy branch.
/// @details This is a copy-work budget, not a formatting capacity or a promise of direct register return. Optional
///          lvalue copies additionally pass the model-specific aggregate-result policy; a non-lvalue may need the
///          bounded copy solely to establish ownership even when the compiler lowers its inevitable return through
///          caller storage.
inline constexpr ::std::size_t io_print_forward_transport_max_value_size{
	::fast_io::details::print_forward_transport_max_value_size};

/// @brief Selects the category-aware value representation shared by print semantic manipulators.
/// @details Trivial copyability alone is not a cost proof: a large pack or condition can be trivial while copying its
///          complete tuple at every semantic boundary. A true lvalue delegates to the shared conservative result
///          policy, while a non-lvalue may take the bounded ownership-copy rescue documented above. Other lvalues use
///          one reference wrapper. The compiler still performs the final ABI classification. The ordered completeness
///          gate is also semantically necessary: a forward-declared lvalue can be transported by reference, but asking
///          a standard triviality trait to classify its referent is not required to be valid.
template <typename T>
inline constexpr bool io_print_forward_transport_by_value = []() constexpr {
	if constexpr (!::fast_io::details::io_print_complete_object<T>)
	{
		return false;
	}
	else
	{
		return ::fast_io::details::print_forward_result_copied_from_named_value<T>();
	}
}();

/// @brief Tests whether character-aware normalization must preserve a borrowed producer's object identity.
/// @details A retained scatter may point into the producer itself. Copying a small lvalue and then retaining the
///          descriptor beyond that copy's lifetime is not justified by the ordinary borrowed marker. The independent
///          copy-stable marker is the only proof that the generic ABI-small value rule remains sound for such a source.
template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_borrowed_source_requires_identity = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	return ((::fast_io::scatter_printable_for<char_type, no_cvref_t &> &&
			 ::fast_io::borrowed_scatter_source<char_type, no_cvref_t>) ||
			((::fast_io::reserve_scatters_printable<char_type, no_cvref_t &> ||
			  ::fast_io::dynamic_reserve_scatters_printable<char_type, no_cvref_t &>) &&
			 ::fast_io::borrowed_reserve_scatters_source<char_type, no_cvref_t>)) &&
		   !::fast_io::copy_stable_borrowed_print_source<char_type, no_cvref_t>;
}();

template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_transport_by_value_for = []() constexpr {
	// The provenance CPOs describe complete printable objects. Do not instantiate that protocol graph when the
	// completeness/value gate has already proved that this argument cannot be copied as an ABI-small value.
	if constexpr (!::fast_io::details::io_print_forward_transport_by_value<T>)
	{
		return false;
	}
	else
	{
		return !::fast_io::details::io_print_forward_borrowed_source_requires_identity<char_type, T>;
	}
}();

/// @brief Proves that ordinary value-or-reference normalization has a valid executed branch.
/// @details Lvalues either use the already-proved ABI-small copy or one exact-reference wrapper. Every other category
///          must become owned storage; completeness is ordered before `constructible_from` so an incomplete rvalue is
///          constraint-false rather than a standard-library diagnostic.
template <typename T>
inline constexpr bool io_print_forward_transportable = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::details::io_print_forward_transport_by_value<T> ||
				  ::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else if constexpr (!::fast_io::details::io_print_complete_object<T>)
	{
		return false;
	}
	else
	{
		return ::std::constructible_from<no_cvref_t, T &&>;
	}
}();

/// @brief Character-aware counterpart of `io_print_forward_transportable`.
/// @details The only different branch is the borrowed-source identity override. It can turn a nominal small-value
///          copy into an exact-reference wrapper for an lvalue, or into ordinary owned construction for a non-lvalue,
///          so transportability is proved after that override is applied.
template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_transportable_for = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::details::io_print_forward_transport_by_value_for<char_type, T> ||
				  ::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else if constexpr (!::fast_io::details::io_print_complete_object<T>)
	{
		return false;
	}
	else
	{
		return ::std::constructible_from<no_cvref_t, T &&>;
	}
}();

/// @brief Computes the exception contract of the value-or-reference-wrapper print transport.
/// @details The downstream print and concat engines intentionally pass normalized arguments by value. An ABI-small
///          trivial object is copied, while another lvalue is carried by `parameter<T>` without copying its referent.
///          An rvalue must become an owned object because a reference to this helper's parameter would escape at return.
template <typename T>
inline constexpr bool io_print_forward_transport_nothrow = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (!::fast_io::details::io_print_forward_transportable<T>)
	{
		return false;
	}
	else if constexpr (::fast_io::details::io_print_forward_transport_by_value<T>)
	{
		return noexcept(static_cast<no_cvref_t>(
			::std::declval<::std::remove_reference_t<T> &>()));
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		// Aggregate construction stores only the already-existing reference. It cannot invoke the referent's copy,
		// move, allocation, or validation code.
		return true;
	}
	else
	{
		return ::std::is_nothrow_constructible_v<no_cvref_t, T &&>;
	}
}();

#if defined(__HERBCEPTIONS__)
/// @brief Classifies the deterministic effect of the exact value/reference transport branch.
/// @details The lvalue-wrapper arm stores one existing reference and therefore has no construction effect. The other
///          arms query the expression actually returned, preserving the ABI-small by-value policy rather than changing
///          it merely to accommodate the error mechanism.
template <typename T>
inline constexpr bool io_print_forward_transport_herbceptions_throws = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (!::fast_io::details::io_print_forward_transportable<T>)
	{
		return false;
	}
	else if constexpr (::fast_io::details::io_print_forward_transport_by_value<T>)
	{
		return throws((static_cast<no_cvref_t>(
			::std::declval<::std::remove_reference_t<T> &>())));
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return false;
	}
	else
	{
		return ::std::is_herbceptions_throws_constructible_v<no_cvref_t, T &&>;
	}
}();
#endif

/// @brief Converts one alias/status-forward result into the by-value transport used by the print engines.
/// @details This is deliberately separate from the ADL dispatcher. In particular, a status customization is first
///          evaluated with its exact value category and only its result is normalized. Consequently a noncopyable
///          lvalue result becomes `parameter<exact-reference>` rather than being copied at the first by-value decay
///          boundary. The compact reference wrapper lets the established control, semantic, and concat paths remain
///          ordinary by-value graphs without claiming that every ABI returns the wrapper in a register.
template <typename T>
	requires(::fast_io::details::io_print_forward_transportable<T>)
inline constexpr auto io_print_forward_transport(T &&t)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::io_print_forward_transport_herbceptions_throws<T>),
		::fast_io::details::io_print_forward_transport_nothrow<T>)
{
	// The two effect proofs describe the same selected construction without changing ownership or value category.
	// Conditional `throws(false)` therefore keeps the ordinary ABI, while a fallible construction gains precisely one
	// discriminator; standard compilers retain the selected construction's exact noexcept result.
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::details::io_print_forward_transport_by_value<T>)
	{
		return static_cast<no_cvref_t>(t);
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return ::fast_io::parameter<T>{t};
	}
	else
	{
		return no_cvref_t(::std::forward<T>(t));
	}
}

template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_transport_nothrow_for = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (!::fast_io::details::io_print_forward_transportable_for<char_type, T>)
	{
		return false;
	}
	else if constexpr (::fast_io::details::io_print_forward_transport_by_value_for<char_type, T>)
	{
		return noexcept(static_cast<no_cvref_t>(
			::std::declval<::std::remove_reference_t<T> &>()));
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else
	{
		return ::std::is_nothrow_constructible_v<no_cvref_t, T &&>;
	}
}();

#if defined(__HERBCEPTIONS__)
template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_transport_herbceptions_throws_for = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (!::fast_io::details::io_print_forward_transportable_for<char_type, T>)
	{
		return false;
	}
	else if constexpr (::fast_io::details::io_print_forward_transport_by_value_for<char_type, T>)
	{
		return throws((static_cast<no_cvref_t>(
			::std::declval<::std::remove_reference_t<T> &>())));
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return false;
	}
	else
	{
		return ::std::is_herbceptions_throws_constructible_v<no_cvref_t, T &&>;
	}
}();
#endif

/// @brief Applies character-aware value/reference decay without invalidating a retained borrowed descriptor.
template <::std::integral char_type, typename T>
	requires(::fast_io::details::io_print_forward_transportable_for<char_type, T>)
inline constexpr auto io_print_forward_transport_for(T &&t)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::io_print_forward_transport_herbceptions_throws_for<char_type, T>),
		::fast_io::details::io_print_forward_transport_nothrow_for<char_type, T>)
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::details::io_print_forward_transport_by_value_for<char_type, T>)
	{
		return static_cast<no_cvref_t>(t);
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return ::fast_io::parameter<T>{t};
	}
	else
	{
		return no_cvref_t(::std::forward<T>(t));
	}
}

/// @brief Computes the exception contract of character-dependent print forwarding.
template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_nothrow = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t>)
	{
		return true;
	}
	else if constexpr (::fast_io::status_io_print_forwardable<char_type, T>)
	{
		using forwarded_type = decltype(status_io_print_forward(
			::fast_io::io_alias_type<char_type>, ::std::declval<T>()));
		return noexcept(status_io_print_forward(
				   ::fast_io::io_alias_type<char_type>, ::std::declval<T>())) &&
			   ::fast_io::details::io_print_forward_transport_nothrow_for<char_type, forwarded_type>;
	}
	else
	{
		return ::fast_io::details::io_print_forward_transport_nothrow_for<char_type, T>;
	}
}();

#if defined(__HERBCEPTIONS__)
/// @brief Computes the Herbception effect of status forwarding followed by the selected decay transport.
/// @details Both expressions belong to one normalization transaction. A fallible status CPO and a fallible owned-result
///          construction therefore make the conditional specification true, while a borrowed or ABI-small safe result
///          keeps the condition false and remains on the plain ABI.
template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_herbceptions_throws = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t>)
	{
		return false;
	}
	else if constexpr (::fast_io::status_io_print_forwardable<char_type, T>)
	{
		using forwarded_type = decltype(status_io_print_forward(
			::fast_io::io_alias_type<char_type>, ::std::declval<T>()));
		return throws((status_io_print_forward(
				   ::fast_io::io_alias_type<char_type>, ::std::declval<T>()))) ||
			   ::fast_io::details::io_print_forward_transport_herbceptions_throws_for<
				   char_type, forwarded_type>;
	}
	else
	{
		return ::fast_io::details::io_print_forward_transport_herbceptions_throws_for<char_type, T>;
	}
}();
#endif

/// @brief Admits the selected print-alias branch without instantiating ownership of an incomplete rvalue.
/// @details A valid ADL alias has already proved its result transport at the CPO boundary. Ordinary lvalues need only
///          retain their reference, while an ordinary rvalue follows the exact owning construction in
///          `io_print_alias`. An invalid customization is ignored like any other unrecognized protocol, but its source
///          still must satisfy the ordinary fallback branch.
template <typename T>
inline constexpr bool io_print_alias_admissible = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t>)
	{
		return true;
	}
	else if constexpr (::fast_io::alias_printable<T>)
	{
		// The alias CPO owns its declared result category. A deterministic-error ABI must carry a reference as a
		// reference (address plus discriminator), exactly as a plain call does; library constraints must not replace
		// that language-level identity contract with an owned copy.
		return true;
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else if constexpr (!::fast_io::details::io_print_complete_object<T>)
	{
		return false;
	}
	else
	{
		return ::std::constructible_from<no_cvref_t, T &&>;
	}
}();

/// @brief Proves that `io_print_forward` can execute the branch selected after status recognition.
template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_admissible = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t>)
	{
		return true;
	}
	else if constexpr (::fast_io::status_io_print_forwardable<char_type, T>)
	{
		// The status concept includes the exact non-lvalue ownership proof; an lvalue result is deliberately borrowed
		// and remains representable by the deterministic-error calling convention.
		return true;
	}
	else
	{
		return ::fast_io::details::io_print_forward_transportable_for<char_type, T>;
	}
}();

/// @brief Proves that scan-alias normalization can execute its selected transport branch.
/// @details Function and valid alias branches construct their own known result. Every lvalue fallback is borrowed,
///          including an immovable manipulator. A non-lvalue fallback must instead cross the return boundary as owned
///          `remove_reference_t<T>` storage; completeness is tested before the constructibility trait so a forward-
///          declared or immovable rvalue makes the call constraint-false rather than diagnosing inside the function.
template <typename T>
inline constexpr bool io_scan_alias_admissible = []() constexpr {
	using no_ref_t = ::std::remove_reference_t<T>;
	if constexpr (::std::is_function_v<no_ref_t>)
	{
		return true;
	}
	else if constexpr (::fast_io::alias_scannable<T>)
	{
		// Preserve the customization's exact result category. In particular, a fallible reference is a valid borrowed
		// alias under the language ABI and must not be converted to an owned scanner proxy by this admission layer.
		return true;
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else if constexpr (!::fast_io::details::io_print_complete_object<no_ref_t>)
	{
		return false;
	}
	else
	{
		return ::std::constructible_from<no_ref_t, T &&>;
	}
}();

/// @brief Proves that scan status forwarding can execute its selected borrow-or-own transport.
/// @details The status concept already proves that a non-reference result can become owned storage. Reference results
///          retain identity through the deterministic-error ABI and consequently need no construction predicate here.
template <::std::integral char_type, typename T>
inline constexpr bool io_scan_forward_admissible = []() constexpr {
	if constexpr (::fast_io::status_io_scan_forwardable<char_type, T>)
	{
		return true;
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else if constexpr (!::fast_io::details::io_print_complete_object<T>)
	{
		return false;
	}
	else
	{
		return ::std::constructible_from<::std::remove_cvref_t<T>, T &&>;
	}
}();

/// @brief Classifies the traditional exception effect of the exact scan-forward normalization transaction.
/// @details A conditional Herbception specification may select its plain ABI only if both the status CPO and any
///          required ownership construction are non-throwing. The expression categories mirror the function body;
///          in particular, an lvalue status result is returned directly and is never copied merely to prove noexcept.
template <::std::integral char_type, typename T>
inline constexpr bool io_scan_forward_nothrow = []() constexpr {
	if constexpr (!::fast_io::details::io_scan_forward_admissible<char_type, T>)
	{
		return false;
	}
	else if constexpr (::fast_io::status_io_scan_forwardable<char_type, T>)
	{
		using result_type = decltype(status_io_scan_forward(
			::fast_io::io_alias_type<char_type>, ::std::declval<T>()));
		if constexpr (::std::is_lvalue_reference_v<result_type>)
		{
			return noexcept(status_io_scan_forward(
				::fast_io::io_alias_type<char_type>, ::std::declval<T>()));
		}
		else
		{
			return noexcept(::std::remove_cvref_t<result_type>(status_io_scan_forward(
				::fast_io::io_alias_type<char_type>, ::std::declval<T>())));
		}
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else
	{
		return ::std::is_nothrow_constructible_v<::std::remove_cvref_t<T>, T &&>;
	}
}();

/// @brief Classifies the ordinary exception effect of the selected scan-alias branch.
/// @details The proof deliberately includes rvalue ownership construction but leaves lvalue transports as references.
///          This keeps a no-failure Herb specialization on the ordinary ABI without weakening the historical
///          potentially-throwing signature for a CPO or constructor that can actually unwind.
template <typename T>
inline constexpr bool io_scan_alias_nothrow = []() constexpr {
	using no_ref_t = ::std::remove_reference_t<T>;
	if constexpr (!::fast_io::details::io_scan_alias_admissible<T>)
	{
		return false;
	}
	else if constexpr (::std::is_function_v<no_ref_t>)
	{
		return true;
	}
	else if constexpr (::fast_io::alias_scannable<T>)
	{
		return noexcept(scan_alias_define(::fast_io::io_alias, ::std::declval<T>()));
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else if constexpr (::fast_io::manipulator<no_ref_t>)
	{
		return ::std::is_nothrow_constructible_v<no_ref_t, T &&>;
	}
	else
	{
		return noexcept(::fast_io::parameter<no_ref_t>{::std::declval<T>()});
	}
}();

#if defined(__HERBCEPTIONS__)
template <::std::integral char_type, typename T>
inline constexpr bool io_scan_forward_herbceptions_throws = []() constexpr {
	if constexpr (!::fast_io::details::io_scan_forward_admissible<char_type, T>)
	{
		return false;
	}
	else if constexpr (::fast_io::status_io_scan_forwardable<char_type, T>)
	{
		using result_type = decltype(status_io_scan_forward(
			::fast_io::io_alias_type<char_type>, ::std::declval<T>()));
		if constexpr (::std::is_lvalue_reference_v<result_type>)
		{
			return throws((status_io_scan_forward(
				::fast_io::io_alias_type<char_type>, ::std::declval<T>())));
		}
		else
		{
			return throws((::std::remove_cvref_t<result_type>(status_io_scan_forward(
				::fast_io::io_alias_type<char_type>, ::std::declval<T>()))));
		}
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return false;
	}
	else
	{
		return ::std::is_herbceptions_throws_constructible_v<::std::remove_cvref_t<T>, T &&>;
	}
}();

template <typename T>
inline constexpr bool io_scan_alias_herbceptions_throws = []() constexpr {
	using no_ref_t = ::std::remove_reference_t<T>;
	if constexpr (::std::is_function_v<no_ref_t>)
	{
		return false;
	}
	else if constexpr (::fast_io::alias_scannable<T>)
	{
		return throws((scan_alias_define(::fast_io::io_alias, ::std::declval<T>())));
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return false;
	}
	else
	{
		return ::std::is_herbceptions_throws_constructible_v<no_ref_t, T &&>;
	}
}();
#endif

} // namespace details

/// @brief Applies the print-alias customization for the argument's actual value category.
/// @details The alias probe intentionally uses `T`, not `remove_cvref_t<T>`.  An alias may be valid only for a
///          mutable lvalue, or may provide distinct lvalue and rvalue overloads; probing a synthesized rvalue would
///          recognize the wrong protocol.  Alias CPO results retain their declared value category, which permits a
///          noncopyable lvalue proxy.  An ordinary rvalue is instead materialized as a value: returning the forwarding
///          reference to this function parameter would leave a dangling reference as soon as the helper returned.
///          No unconditional `noexcept` is stated because an alias is allowed to validate or allocate.
template <typename T>
	requires(::fast_io::details::io_print_alias_admissible<T>)
inline constexpr decltype(auto) io_print_alias(T &&t)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::io_print_alias_herbceptions_throws<T>),
		::fast_io::details::io_print_alias_nothrow<T>)
{
	// The ADL branch owns both its exact success category and its deterministic effect. A conditional specification
	// transports `T&`/`T&&` as references on success and never substitutes an owned value for compiler convenience.
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t>)
	{
		return ::fast_io::details::cannot_output_type{};
	}
	else if constexpr (alias_printable<T>)
	{
		return print_alias_define(io_alias, ::std::forward<T>(t));
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return (t);
	}
	else
	{
		return no_cvref_t(::std::forward<T>(t));
	}
}

/// @brief Applies character-dependent print forwarding and produces a cheap by-value transport.
/// @details A status-forward customization is called before transport selection, so overload resolution observes the
///          original aliased value category. Its result is the final printable representation: it is transported
///          directly and is never passed through `io_print_alias` a second time. The result then follows the same rule
///          as an ordinary argument: safely copyable small values remain values, while nontrivial or noncopyable
///          lvalues become `parameter<exact-reference>` and rvalues become owned values. This invariant is what
///          permits every downstream dispatcher to remain
///          by-value without erasing mutability, const qualification, or noncopyable proxy identity. Customization
///          exceptions propagate because neither the status-forward nor alias protocol requires a non-throwing CPO.
template <::std::integral char_type, typename T>
	requires(::fast_io::details::io_print_forward_admissible<char_type, T>)
inline constexpr auto io_print_forward(T &&t)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::io_print_forward_herbceptions_throws<char_type, T>),
		::fast_io::details::io_print_forward_nothrow<char_type, T>)
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t>)
	{
		return ::fast_io::details::cannot_output_type{};
	}
	else if constexpr (status_io_print_forwardable<char_type, T>)
	{
		return ::fast_io::details::io_print_forward_transport_for<char_type>(
			status_io_print_forward(io_alias_type<char_type>, ::std::forward<T>(t)));
	}
	else
	{
		return ::fast_io::details::io_print_forward_transport_for<char_type>(::std::forward<T>(t));
	}
}

/// @brief Applies character-dependent scan forwarding without copying an existing proxy reference.
/// @details Both the source expression and a status-forward result obey the same borrow-or-own rule. An lvalue result
///          retains its exact reference identity, which permits noncopyable proxy customizations. A prvalue or xvalue
///          result is materialized as a value so this helper never returns a reference to its parameter or to a status
///          customization's expiring subobject. `status_io_scan_forwardable` proves this exact construction before the
///          branch is selected, making an incomplete or immovable non-lvalue result a substitution failure rather than
///          a delayed body diagnostic. Custom forwarding exceptions remain visible because the protocol does not promise
///          `noexcept`.
template <::std::integral char_type, typename T>
#if defined(__HERBCEPTIONS__)
	requires(::fast_io::details::io_scan_forward_admissible<char_type, T>)
#endif
inline constexpr decltype(auto) io_scan_forward(T &&t)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::io_scan_forward_herbceptions_throws<char_type, T>),
		::fast_io::details::io_scan_forward_nothrow<char_type, T>)
{
	// This normalization boundary invokes one open ADL CPO and may materialize its result. The conditional specification
	// represents that exact transaction without changing a reference result's identity; ordinary mode deliberately
	// remains potentially throwing, matching the historical contract.
	if constexpr (status_io_scan_forwardable<char_type, T>)
	{
		using result_type = decltype(status_io_scan_forward(
			io_alias_type<char_type>, ::std::forward<T>(t)));
		if constexpr (::std::is_lvalue_reference_v<result_type>)
		{
			return status_io_scan_forward(io_alias_type<char_type>, ::std::forward<T>(t));
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(status_io_scan_forward(
				io_alias_type<char_type>, ::std::forward<T>(t)));
		}
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return (t);
	}
	else
	{
		return ::std::remove_cvref_t<T>(::std::forward<T>(t));
	}
}

/// @brief Normalizes a public scan target while preserving alias reference semantics.
/// @details A custom alias may return a noncopyable reference proxy; `decltype(auto)` retains it. Ordinary targets are
///          wrapped in a small parameter transport: an lvalue transport stores its exact reference and a direct rvalue
///          transport owns the object. Existing manipulators follow the same borrow-or-own rule. Returning a reference
///          to either kind of rvalue function parameter would dangle immediately after this helper call. Public `scan`
///          normally presents its named target as an lvalue, so ownership affects only direct normalization and does
///          not add a copy to the common path. The function is not unconditionally `noexcept`, because an alias
///          customization or rvalue construction may validate, allocate, or throw.
template <typename T>
	requires(::fast_io::details::io_scan_alias_admissible<T>)
inline constexpr decltype(auto) io_scan_alias(T &&t)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::io_scan_alias_herbceptions_throws<T>),
		::fast_io::details::io_scan_alias_nothrow<T>)
{
	// Alias validation and owned rvalue construction are one user-extensible effect transaction. Conditional `throws`
	// preserves its precise error ABI, including a plain ABI for the no-failure case, while ordinary mode retains the
	// historical potentially-throwing signature.
	using no_ref_t = ::std::remove_reference_t<T>;
	if constexpr (::std::is_function_v<no_ref_t>)
	{
		return ::fast_io::details::cannot_output_type{};
	}
	else if constexpr (alias_scannable<T>)
	{
		// Recognition and invocation use the same value category. The former bare-type probe tested a named lvalue but
		// could then call an rvalue-only expression (or the reverse), producing a hard error after concept admission.
		return scan_alias_define(io_alias, ::std::forward<T>(t));
	}
	else if constexpr (manipulator<no_ref_t>)
	{
		if constexpr (::std::is_lvalue_reference_v<T &&>)
		{
			// The lvalue branch returns the exact existing object, so the classification marker needs no copy/move proof.
			return (t);
		}
		else
		{
			// The entry constraint proves this exact `no_ref_t(T&&)` expression before the owning branch is instantiated.
			// Ownership is mandatory here because returning a reference to the forwarding parameter would dangle.
			return no_ref_t(::std::forward<T>(t));
		}
	}
	else
	{
		if constexpr (::std::is_lvalue_reference_v<T &&>)
		{
			return parameter<no_ref_t &>{t};
		}
		else
		{
			return parameter<no_ref_t>{::std::forward<T>(t)};
		}
	}
}

} // namespace fast_io
