#pragma once

namespace fast_io
{

template <::std::integral char_type, manipulators::scalar_flags flags>
	requires(flags.alphabet)
inline constexpr basic_io_scatter_t<char_type>
print_scatter_define(basic_lc_all<char_type> const *__restrict all,
					 manipulators::scalar_manip_t<flags, bool> val) noexcept
{
	if (val.reference)
	{
		return ::fast_io::details::lc_resolve_scatter(all, all->messages.yesstr);
	}
	else
	{
		return ::fast_io::details::lc_resolve_scatter(all, all->messages.nostr);
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags>
	requires(flags.alphabet)
inline constexpr ::std::true_type print_lc_borrowed_scatter_source(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, bool>>) noexcept
{
	// Both descriptors reside in the immutable locale aggregate owned by the synchronous imbuer. Re-observing one
	// unchanged boolean under that same locale selects the same yes/no descriptor, so lifetime and replay are proved.
	return {};
}

} // namespace fast_io
