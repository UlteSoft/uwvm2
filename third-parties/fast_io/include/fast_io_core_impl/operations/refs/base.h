#pragma once

/*
 * Public stream-reference normalization boundary (CPO level).
 *
 * `input_stream_ref`, `output_stream_ref`, and `io_stream_ref` project an
 * arbitrary user handle to the observer used by operation algorithms. Each CPO
 * is invoked exactly once at the public boundary. A mutable lvalue result may
 * remain borrowed; a prvalue result is owned once; explicit semantic and ABI
 * proofs govern any repeated by-value transport. Deeper `operations::decay`
 * code must consume that normalized object rather than reopen the CPO.
 */

namespace fast_io::operations
{

namespace defines
{

/// @brief Proves the single borrow-or-own representation shared by directional stream-reference normalization.
/// @details `input_stream_ref`, `output_stream_ref`, and `io_stream_ref` use a category-aware `decltype(auto)` result. A
///          move-only prvalue is valid: high-level scan/print dispatch owns it once and passes a stable lvalue to deeper
///          strategy layers. Every admitted mutable non-cv lvalue remains the exact reference returned by its CPO;
///          trivial special members and a small ABI representation prove only copy mechanics, never that replacing one
///          observer identity with another preserves cursor or buffering semantics. Const lvalues still require
///          materialization because input/output protocols generally mutate cursor state through the normalized
///          observer. An immovable same-type prvalue is valid as well: both the explicit value branch
///          and its destination are initialized under C++17 guaranteed copy elision, so asking
///          `constructible_from<T, T>` would incorrectly require a move which the executed program never performs. A
///          reference result selected for materialization is different: copying from an lvalue or xvalue really does
///          invoke the corresponding constructor and must be checked. Whether later primitive layers should continue
///          copying the normalized value is a separate semantic-and-argument-cost proof below.
///
///          `is_object` alone accepts forward declarations. The explicit completeness branch is deliberately ordered
///          before either type trait or construction concept because their standard-library implementations may issue
///          a hard diagnostic for an incomplete class instead of making the enclosing requirement false.
template <typename result_type>
inline consteval bool storable_stream_ref_result_object() noexcept
{
	using result_value_type = ::std::remove_cvref_t<result_type>;
	if constexpr (!::std::is_object_v<::std::remove_reference_t<result_type>> ||
					 !requires { sizeof(result_value_type); })
	{
		return false;
	}
	else
	{
		if constexpr (::std::is_lvalue_reference_v<result_type> &&
					  !::std::is_const_v<::std::remove_reference_t<result_type>> &&
					  !::std::is_volatile_v<::std::remove_reference_t<result_type>>)
		{
			return true;
		}
		else if constexpr (::std::is_reference_v<result_type>)
		{
			return ::std::constructible_from<result_value_type, result_type>;
		}
		else
		{
			return true;
		}
	}
}

/// @brief Detects an explicit semantic proof that copies of a normalized stream proxy are substitutable.
/// @details ABI size, trivial special members, and even bitwise equality say nothing about observer identity. A compact
///          proxy may store its cursor directly, in which case a by-value primitive mutates a discarded copy. The ADL
///          marker is therefore mandatory before any reusable concept may authorize repeated value propagation. Its
///          author promises that every operation observable through one copy is equivalent to the same operation through
///          the original, for example because all mutable state is held in a shared external control block. This semantic
///          contract is deliberately independent of `abi_value_transport_force_direct`, which proves lowering only.
template <typename value_type>
concept stream_ref_value_transport_safe = requires {
	{
		stream_ref_value_transport_safe_define(::fast_io::io_type_t<value_type>{})
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Admits repeated by-value primitive propagation only after semantic and argument-cost proofs.
/// @details This refinement is intentionally stronger than one-time stream normalization. The explicit marker above
///          proves identity-insensitive copy substitution. Trivial copy/move construction and destruction then avoid
///          hidden calls and the C++ ABI's indirect non-trivial aggregate rules; the reflection-free target envelope
///          distinguishes Microsoft x64, SysV AMD64, AAPCS, RISC-V, LoongArch, MIPS, PowerPC64, SPARC V9, and
///          conservative indirect/unknown families. Construction from a named lvalue mirrors every recursive primitive
///          call. This remains a policy filter rather than a complete ABI classifier: member register classes and final
///          lowering belong to the compiler. Callers that normalize once and borrow thereafter must use only the
///          `storable_*` concepts and must not inherit either value-copy requirement.
template <typename result_type>
inline consteval bool abi_value_stream_ref_result_object() noexcept
{
	using result_value_type = ::std::remove_cvref_t<result_type>;
	if constexpr (!::fast_io::operations::defines::storable_stream_ref_result_object<result_type>())
	{
		return false;
	}
	else
	{
		return ::fast_io::operations::defines::stream_ref_value_transport_safe<
				   result_value_type> &&
			   ::fast_io::details::abi_small_trivial_argument_object<result_value_type>() &&
			   ::std::constructible_from<result_value_type, result_value_type &>;
	}
}

/// @brief Proves that an input stream-reference result can be materialized once at the normalized entry boundary.
/// @details Stream-reference CPOs may return values or stable references. The public boundary creates one
///          `remove_cvref_t` owner for value transport or preserves an admitted non-cv lvalue, and deeper high-level
///          strategies borrow that result. This concept proves that normalization plus the input character-domain
///          shape; it intentionally says nothing about whether a primitive may repeatedly copy an owner. Such a
///          primitive must additionally use the `abi_value_*` refinement below.
template <typename result_type>
concept storable_input_stream_ref_result =
	::fast_io::operations::defines::storable_stream_ref_result_object<result_type>() && requires {
		typename ::std::remove_cvref_t<result_type>::input_char_type;
		requires ::std::integral<typename ::std::remove_cvref_t<result_type>::input_char_type>;
	};

/// @brief Output-direction counterpart of `storable_input_stream_ref_result`.
template <typename result_type>
concept storable_output_stream_ref_result =
	::fast_io::operations::defines::storable_stream_ref_result_object<result_type>() && requires {
		typename ::std::remove_cvref_t<result_type>::output_char_type;
		requires ::std::integral<typename ::std::remove_cvref_t<result_type>::output_char_type>;
	};

/// @brief Joint-direction stream-reference result used by operations that share input and output state.
/// @details Both character domains are checked on the same normalized object; independently valid directional proxies
///          are not evidence for an io-level reference.
template <typename result_type>
concept storable_io_stream_ref_result =
	::fast_io::operations::defines::storable_input_stream_ref_result<result_type> &&
	::fast_io::operations::defines::storable_output_stream_ref_result<result_type>;

template <typename result_type>
concept abi_value_input_stream_ref_result =
	::fast_io::operations::defines::storable_input_stream_ref_result<result_type> &&
	::fast_io::operations::defines::abi_value_stream_ref_result_object<result_type>();

template <typename result_type>
concept abi_value_output_stream_ref_result =
	::fast_io::operations::defines::storable_output_stream_ref_result<result_type> &&
	::fast_io::operations::defines::abi_value_stream_ref_result_object<result_type>();

template <typename result_type>
concept abi_value_io_stream_ref_result =
	::fast_io::operations::defines::storable_io_stream_ref_result<result_type> &&
	::fast_io::operations::defines::abi_value_stream_ref_result_object<result_type>();

/// @brief Selects stable-reference normalization for every admitted mutable non-cv CPO lvalue result.
/// @details The CPO author supplied both the referent's lifetime and the lvalue category. Preserving that category is the
///          only generally valid substitution: a one-word trivial observer may keep its cursor in the observer object,
///          so copying it can redirect all mutations into a discarded surrogate. No ABI size class can prove
///          identity-insensitivity. Prvalues and xvalues still create an owner because a reference to the CPO result may
///          expire at the full-expression boundary; const/volatile lvalues retain materialization because ordinary
///          cursor and buffer protocols require a mutable normalized observer. Thus category and cv-qualification, not
///          transport cost, completely determine this normalization branch.
template <typename result_type>
inline constexpr bool stream_ref_result_borrows_lvalue =
	::std::is_lvalue_reference_v<result_type> &&
	!::std::is_const_v<::std::remove_reference_t<result_type>> &&
	!::std::is_volatile_v<::std::remove_reference_t<result_type>>;

template <typename T>
concept has_input_stream_ref_define = requires(T &&t) {
	input_stream_ref_define(::fast_io::freestanding::forward<T>(t));
	requires ::fast_io::operations::defines::storable_input_stream_ref_result<decltype(
		input_stream_ref_define(::fast_io::freestanding::forward<T>(t)))>;
};

template <typename T>
concept has_output_stream_ref_define = requires(T &&t) {
	output_stream_ref_define(::fast_io::freestanding::forward<T>(t));
	requires ::fast_io::operations::defines::storable_output_stream_ref_result<decltype(
		output_stream_ref_define(::fast_io::freestanding::forward<T>(t)))>;
};

/// @brief Recognizes the input projection of a shared `io_stream_ref_define` customization.
/// @details An `io_stream_ref_define` result is not required to expose both character domains merely because the CPO
///          has the joint spelling. Existing adapters use it as a common fallback for one-directional views. Input
///          normalization must therefore prove only the input shape it will consume; requiring an unrelated
///          `output_char_type` makes a valid input object miss the source overload and become a scan target instead.
template <typename T>
concept has_input_compatible_io_stream_ref_define = requires(T &&t) {
	io_stream_ref_define(::fast_io::freestanding::forward<T>(t));
	requires ::fast_io::operations::defines::storable_input_stream_ref_result<decltype(
		io_stream_ref_define(::fast_io::freestanding::forward<T>(t)))>;
};

/// @brief Recognizes the output projection of a shared `io_stream_ref_define` customization.
template <typename T>
concept has_output_compatible_io_stream_ref_define = requires(T &&t) {
	io_stream_ref_define(::fast_io::freestanding::forward<T>(t));
	requires ::fast_io::operations::defines::storable_output_stream_ref_result<decltype(
		io_stream_ref_define(::fast_io::freestanding::forward<T>(t)))>;
};

/// @brief Recognizes the genuinely joint projection used by operations that consume both directions together.
template <typename T>
concept has_io_stream_ref_define = requires(T &&t) {
	io_stream_ref_define(::fast_io::freestanding::forward<T>(t));
	requires ::fast_io::operations::defines::storable_io_stream_ref_result<decltype(
		io_stream_ref_define(::fast_io::freestanding::forward<T>(t)))>;
};

template <typename T>
concept has_input_or_io_stream_ref_define =
	::fast_io::operations::defines::has_input_stream_ref_define<T> ||
	::fast_io::operations::defines::has_input_compatible_io_stream_ref_define<T>;

template <typename T>
concept has_output_or_io_stream_ref_define =
	::fast_io::operations::defines::has_output_stream_ref_define<T> ||
	::fast_io::operations::defines::has_output_compatible_io_stream_ref_define<T>;

/// @brief Classifies the traditional exception effect of input stream-reference normalization.
/// @details The query models the selected CPO and, for every non-borrowed category, the exact one-time owner
///          construction. This is the second half of the conditional error contract: `throws(false)` is permitted only
///          when this proof and the independent deterministic-channel proof are both false/no-failure respectively.
template <typename T>
inline constexpr bool input_stream_ref_nothrow = []() constexpr {
	if constexpr (::fast_io::operations::defines::has_input_stream_ref_define<T>)
	{
		using result_type = decltype(input_stream_ref_define(::std::declval<T>()));
		if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
		{
			return noexcept(input_stream_ref_define(::std::declval<T>()));
		}
		else
		{
			return noexcept(::std::remove_cvref_t<result_type>(
				input_stream_ref_define(::std::declval<T>())));
		}
	}
	else
	{
		using result_type = decltype(io_stream_ref_define(::std::declval<T>()));
		if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
		{
			return noexcept(io_stream_ref_define(::std::declval<T>()));
		}
		else
		{
			return noexcept(::std::remove_cvref_t<result_type>(
				io_stream_ref_define(::std::declval<T>())));
		}
	}
}();

/// @brief Output-direction counterpart of `input_stream_ref_nothrow`.
/// @details A reference result is queried as the reference-returning call itself; an owned result includes its explicit
///          materialization. Keeping those cases separate prevents an ABI policy from introducing a diagnostic-only
///          copy or from declaring a legacy-throwing projection `throws(false)`.
template <typename T>
inline constexpr bool output_stream_ref_nothrow = []() constexpr {
	if constexpr (::fast_io::operations::defines::has_output_stream_ref_define<T>)
	{
		using result_type = decltype(output_stream_ref_define(::std::declval<T>()));
		if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
		{
			return noexcept(output_stream_ref_define(::std::declval<T>()));
		}
		else
		{
			return noexcept(::std::remove_cvref_t<result_type>(
				output_stream_ref_define(::std::declval<T>())));
		}
	}
	else
	{
		using result_type = decltype(io_stream_ref_define(::std::declval<T>()));
		if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
		{
			return noexcept(io_stream_ref_define(::std::declval<T>()));
		}
		else
		{
			return noexcept(::std::remove_cvref_t<result_type>(
				io_stream_ref_define(::std::declval<T>())));
		}
	}
}();

/// @brief Classifies both the projection and ownership construction of a joint input/output reference.
template <typename T>
inline constexpr bool io_stream_ref_nothrow = []() constexpr {
	using result_type = decltype(io_stream_ref_define(::std::declval<T>()));
	if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
	{
		return noexcept(io_stream_ref_define(::std::declval<T>()));
	}
	else
	{
		return noexcept(::std::remove_cvref_t<result_type>(
			io_stream_ref_define(::std::declval<T>())));
	}
}();

#if defined(__HERBCEPTIONS__)
/// @brief Classifies the deterministic effect of a directional stream-reference normalization transaction.
/// @details The full materialization expression is queried for owned results so the CPO and its one required
///          construction cannot disagree about the selected ABI. A mutable lvalue result instead remains the exact
///          borrowed reference; the error discriminator is orthogonal to that success-value identity and is therefore
///          propagated by the conditional throws specification without imposing an ownership construction.
template <typename T>
inline constexpr bool input_stream_ref_herbceptions_throws = []() constexpr {
	if constexpr (::fast_io::operations::defines::has_input_stream_ref_define<T>)
	{
		using result_type = decltype(input_stream_ref_define(::std::declval<T>()));
		if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
		{
			return throws((input_stream_ref_define(::std::declval<T>())));
		}
		else
		{
			return throws((::std::remove_cvref_t<result_type>(
				input_stream_ref_define(::std::declval<T>()))));
		}
	}
	else
	{
		using result_type = decltype(io_stream_ref_define(::std::declval<T>()));
		if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
		{
			return throws((io_stream_ref_define(::std::declval<T>())));
		}
		else
		{
			return throws((::std::remove_cvref_t<result_type>(
				io_stream_ref_define(::std::declval<T>()))));
		}
	}
}();

template <typename T>
inline constexpr bool output_stream_ref_herbceptions_throws = []() constexpr {
	if constexpr (::fast_io::operations::defines::has_output_stream_ref_define<T>)
	{
		using result_type = decltype(output_stream_ref_define(::std::declval<T>()));
		if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
		{
			return throws((output_stream_ref_define(::std::declval<T>())));
		}
		else
		{
			return throws((::std::remove_cvref_t<result_type>(
				output_stream_ref_define(::std::declval<T>()))));
		}
	}
	else
	{
		using result_type = decltype(io_stream_ref_define(::std::declval<T>()));
		if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
		{
			return throws((io_stream_ref_define(::std::declval<T>())));
		}
		else
		{
			return throws((::std::remove_cvref_t<result_type>(
				io_stream_ref_define(::std::declval<T>()))));
		}
	}
}();

template <typename T>
inline constexpr bool io_stream_ref_herbceptions_throws = []() constexpr {
	using result_type = decltype(io_stream_ref_define(::std::declval<T>()));
	if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
	{
		return throws((io_stream_ref_define(::std::declval<T>())));
	}
	else
	{
		return throws((::std::remove_cvref_t<result_type>(
			io_stream_ref_define(::std::declval<T>()))));
	}
}();
#endif

} // namespace defines

template <typename T>
	requires(::fast_io::operations::defines::has_input_or_io_stream_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
/// `decltype(auto)` preserves only the explicitly selected stable-lvalue branch. Every other branch spells an explicit
/// value construction, so callers see one owned proxy rather than accidentally extending an xvalue reference.
inline constexpr decltype(auto) input_stream_ref(T &&t)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::operations::defines::input_stream_ref_herbceptions_throws<T>),
		::fast_io::operations::defines::input_stream_ref_nothrow<T>)
{
	// The selected ADL projection and required result materialization are one effect boundary. Its Boolean specification
	// controls only the discriminator ABI; it never changes the exact borrowed success category.
	if constexpr (::fast_io::operations::defines::has_input_stream_ref_define<T>)
	{
		using result_type = decltype(input_stream_ref_define(::fast_io::freestanding::forward<T>(t)));
		if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
		{
			return input_stream_ref_define(::fast_io::freestanding::forward<T>(t));
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(
				input_stream_ref_define(::fast_io::freestanding::forward<T>(t)));
		}
	}
	else
	{
		using result_type = decltype(io_stream_ref_define(::fast_io::freestanding::forward<T>(t)));
		if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
		{
			return io_stream_ref_define(::fast_io::freestanding::forward<T>(t));
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(
				io_stream_ref_define(::fast_io::freestanding::forward<T>(t)));
		}
	}
}

template <typename T>
	requires(::fast_io::operations::defines::has_output_or_io_stream_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) output_stream_ref(T &&t)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::operations::defines::output_stream_ref_herbceptions_throws<T>),
		::fast_io::operations::defines::output_stream_ref_nothrow<T>)
{
	// Preserve the exact output observer category while exposing a deterministic failure raised by either the CPO or
	// owned proxy construction. A false condition is the language-defined plain ABI, not a reason to duplicate the CPO.
	if constexpr (::fast_io::operations::defines::has_output_stream_ref_define<T>)
	{
		using result_type = decltype(output_stream_ref_define(::fast_io::freestanding::forward<T>(t)));
		if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
		{
			return output_stream_ref_define(::fast_io::freestanding::forward<T>(t));
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(
				output_stream_ref_define(::fast_io::freestanding::forward<T>(t)));
		}
	}
	else
	{
		using result_type = decltype(io_stream_ref_define(::fast_io::freestanding::forward<T>(t)));
		if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
		{
			return io_stream_ref_define(::fast_io::freestanding::forward<T>(t));
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(
				io_stream_ref_define(::fast_io::freestanding::forward<T>(t)));
		}
	}
}

template <typename T>
	requires(::fast_io::operations::defines::has_io_stream_ref_define<T>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) io_stream_ref(T &&t)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::operations::defines::io_stream_ref_herbceptions_throws<T>),
		::fast_io::operations::defines::io_stream_ref_nothrow<T>)
{
	// Joint-direction normalization has the same single-evaluation and ownership contract as the directional forms;
	// its effect specification must consequently cover both ADL invocation and result materialization as one operation.
	using result_type = decltype(io_stream_ref_define(::fast_io::freestanding::forward<T>(t)));
	if constexpr (::fast_io::operations::defines::stream_ref_result_borrows_lvalue<result_type>)
	{
		return io_stream_ref_define(::fast_io::freestanding::forward<T>(t));
	}
	else
	{
		return ::std::remove_cvref_t<result_type>(
			io_stream_ref_define(::fast_io::freestanding::forward<T>(t)));
	}
}

} // namespace fast_io::operations
