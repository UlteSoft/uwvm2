#pragma once

namespace fast_io
{

namespace details
{

template <typename T>
concept pack_value_transferable = ::std::is_trivially_copyable_v<::std::remove_cvref_t<T>> &&
#if (defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__)
								  sizeof(::std::remove_cvref_t<T>) <= 8u
#else
								  sizeof(::std::remove_cvref_t<T>) <= (sizeof(::std::size_t) * 2)
#endif
	;

template <typename T>
using pack_alias_type =
	::std::conditional_t<::fast_io::details::alias_return_lvalue_ref<T>,
						 ::std::conditional_t<::fast_io::details::pack_value_transferable<T>,
											  ::std::remove_cvref_t<T>, ::std::remove_cvref_t<T> const &>,
						 ::std::remove_cvref_t<decltype(::fast_io::io_print_alias(::std::declval<T>()))>>;

} // namespace details

namespace manipulators
{

template <typename T1, typename T2>
struct condition;

enum class scalar_placement : char8_t;

template <scalar_placement flags, typename T>
struct width_t;

template <scalar_placement flags, typename T, ::std::integral ch_type>
struct width_ch_t;

template <typename T>
struct width_runtime_t;

template <typename T, ::std::integral ch_type>
struct width_runtime_ch_t;

struct pack_manip_tag_t
{};

template <typename... Args>
struct pack_t
{
	using manip_tag = ::fast_io::manip_tag_t;
	using semantic_tag = pack_manip_tag_t;
	using storage_type = ::fast_io::containers::tuple<Args...>;
	inline static constexpr ::std::size_t size = sizeof...(Args);

	storage_type storage;
};

template <typename... Args>
inline constexpr auto pack(Args &&...args) noexcept
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
	return pack_t<::fast_io::details::pack_alias_type<Args>...>{
		::fast_io::containers::tuple<::fast_io::details::pack_alias_type<Args>...>{
			::fast_io::io_print_alias(::std::forward<Args>(args))...}};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

} // namespace manipulators

namespace details
{

template <typename T>
inline constexpr bool is_print_pack_v = false;

template <typename... Args>
inline constexpr bool is_print_pack_v<::fast_io::manipulators::pack_t<Args...>> = true;

template <typename T>
concept print_pack = is_print_pack_v<::std::remove_cvref_t<T>>;

} // namespace details

} // namespace fast_io
