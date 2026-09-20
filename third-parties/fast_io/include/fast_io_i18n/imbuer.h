#pragma once

namespace fast_io
{

template <typename stm>
struct lc_imbuer
{
	using handle_type = stm;
	using handle_value_type = ::std::remove_cvref_t<handle_type>;
	using char_type = typename handle_value_type::output_char_type;
	using output_char_type = char_type;
	using lc_type = ::fast_io::basic_lc_object<char_type>;
	lc_type const *locale{};
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address) >= 201803
	[[no_unique_address]]
#endif
#endif
	handle_type handle;
};

template <typename stm>
inline constexpr lc_imbuer<stm> &output_stream_ref_define(lc_imbuer<stm> &t) noexcept
{
	// Public print owns an rvalue imbuer in its forwarding-reference parameter and presents that named object here as an
	// lvalue. Borrowing it preserves either the underlying reference member or the single normalized handle owner.
	return t;
}

template <typename stm>
inline constexpr lc_imbuer<stm> &&output_stream_ref_define(lc_imbuer<stm> &&t) noexcept
{
	// A direct low-level rvalue normalization may materialize this result once. Returning the same expiring owner by
	// rvalue reference avoids a CPO-local copy; the common stream-reference normalizer performs the required move.
	return ::std::move(t);
}

template <::std::integral char_type, typename stm>
	requires requires {
		::fast_io::operations::output_stream_ref(::std::declval<stm>());
		requires ::std::same_as<
			char_type,
			typename ::std::remove_cvref_t<decltype(
				::fast_io::operations::output_stream_ref(::std::declval<stm>()))>::output_char_type>;
	}
inline constexpr auto imbue(::fast_io::basic_lc_object<char_type> const &locale, stm &&out)
	noexcept(noexcept(::fast_io::operations::output_stream_ref(::std::forward<stm>(out))))
{
	using result_type = decltype(
		::fast_io::operations::output_stream_ref(::std::forward<stm>(out)));
	using storage_type = ::std::conditional_t<
		::std::is_lvalue_reference_v<result_type>, result_type,
		::std::remove_cvref_t<result_type>>;
	// Mutable lvalue CPO results carry an established lifetime and remain exact references. Every other admitted result
	// is already a one-time owner produced by output_stream_ref and is materialized directly into the aggregate member.
	return lc_imbuer<storage_type>{
		__builtin_addressof(locale),
		::fast_io::operations::output_stream_ref(::std::forward<stm>(out))};
}

} // namespace fast_io
