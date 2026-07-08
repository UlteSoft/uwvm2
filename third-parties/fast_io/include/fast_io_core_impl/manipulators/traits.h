#pragma once

#include "forward.h"

namespace fast_io
{

namespace details
{

template <typename T>
inline constexpr bool is_print_pack_v = false;

template <typename... Args>
inline constexpr bool is_print_pack_v<::fast_io::manipulators::pack_t<Args...>> = true;

template <typename T>
concept print_pack = is_print_pack_v<::std::remove_cvref_t<T>>;

} // namespace details

namespace details::decay
{

template <typename T>
inline constexpr bool print_semantic_condition_v = false;

template <typename T1, typename T2>
inline constexpr bool
	print_semantic_condition_v<::fast_io::manipulators::condition<T1, T2>> = true;

template <typename T>
inline constexpr bool print_semantic_width_v = false;

template <::fast_io::manipulators::scalar_placement placement, typename T>
inline constexpr bool print_semantic_width_v<::fast_io::manipulators::width_t<placement, T>> = true;

template <::fast_io::manipulators::scalar_placement placement, typename T, ::std::integral ch_type>
inline constexpr bool
	print_semantic_width_v<::fast_io::manipulators::width_ch_t<placement, T, ch_type>> = true;

template <typename T>
inline constexpr bool print_semantic_width_v<::fast_io::manipulators::width_runtime_t<T>> = true;

template <typename T, ::std::integral ch_type>
inline constexpr bool print_semantic_width_v<::fast_io::manipulators::width_runtime_ch_t<T, ch_type>> = true;

template <typename T>
inline constexpr bool print_semantic_node_no_parameter_v =
	::fast_io::details::print_pack<T> ||
	::fast_io::details::decay::print_semantic_condition_v<::std::remove_cvref_t<T>> ||
	::fast_io::details::decay::print_semantic_width_v<::std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool print_semantic_parameter_object_v = false;

template <typename T>
inline constexpr bool print_semantic_parameter_object_v<::fast_io::parameter<T>> = true;

template <typename T>
inline constexpr bool print_semantic_parameter_v = false;

template <typename T>
inline constexpr bool print_semantic_parameter_v<::fast_io::parameter<T>> =
	::fast_io::details::decay::print_semantic_node_no_parameter_v<::std::remove_cvref_t<T>>;

template <typename T>
concept print_semantic_node =
	::fast_io::details::decay::print_semantic_node_no_parameter_v<::std::remove_cvref_t<T>> ||
	::fast_io::details::decay::print_semantic_parameter_v<::std::remove_cvref_t<T>>;

template <typename T>
struct print_semantic_width_traits
{};

template <::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_width_traits<::fast_io::manipulators::width_t<placement, T>>
{
	inline static constexpr bool runtime_placement = false;
	inline static constexpr ::fast_io::manipulators::scalar_placement static_placement = placement;
	inline static constexpr bool has_fill_char = false;
};

template <::fast_io::manipulators::scalar_placement placement, typename T, ::std::integral ch_type>
struct print_semantic_width_traits<::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
{
	using fill_char_type = ch_type;
	inline static constexpr bool runtime_placement = false;
	inline static constexpr ::fast_io::manipulators::scalar_placement static_placement = placement;
	inline static constexpr bool has_fill_char = true;
};

template <typename T>
struct print_semantic_width_traits<::fast_io::manipulators::width_runtime_t<T>>
{
	inline static constexpr bool runtime_placement = true;
	inline static constexpr bool has_fill_char = false;
};

template <typename T, ::std::integral ch_type>
struct print_semantic_width_traits<::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
{
	using fill_char_type = ch_type;
	inline static constexpr bool runtime_placement = true;
	inline static constexpr bool has_fill_char = true;
};

template <::std::integral char_type, typename T>
struct print_freestanding_decay_param_okay_single;

template <::std::integral char_type, typename T>
using print_semantic_forwarded_arg_t =
	decltype(::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(::std::declval<T>())));

template <::std::integral char_type, typename T>
struct print_semantic_static_precise_size_impl
{
	inline static constexpr bool available = ::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>;
	inline static constexpr ::std::size_t size = available ? 0u : SIZE_MAX;
};

template <::std::integral char_type, typename T>
struct print_semantic_static_precise_size
	: ::fast_io::details::decay::print_semantic_static_precise_size_impl<char_type, ::std::remove_cvref_t<T>>
{};

template <::std::integral char_type, typename T>
	requires(!::fast_io::details::decay::print_semantic_parameter_object_v<T> &&
			 ::fast_io::static_precise_reserve_printable<char_type, T>)
struct print_semantic_static_precise_size_impl<char_type, T>
{
	inline static constexpr bool available = true;
	inline static constexpr ::std::size_t size{
		print_reserve_static_precise_size(::fast_io::io_reserve_type<char_type, T>)};
};

template <::std::integral char_type, typename T>
struct print_semantic_static_precise_size_impl<char_type, ::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_static_precise_size<char_type, T>
{};

template <::std::integral char_type, typename... Args>
struct print_semantic_static_precise_size_impl<char_type, ::fast_io::manipulators::pack_t<Args...>>
{
	inline static constexpr bool available =
		(::fast_io::details::decay::print_semantic_static_precise_size<
			 char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::available &&
		 ...);

	static consteval ::std::size_t static_size() noexcept
	{
		if constexpr (available)
		{
			::std::size_t total{};
			((total = ::fast_io::details::intrinsics::add_or_overflow_die(
				  total, ::fast_io::details::decay::print_semantic_static_precise_size<
							 char_type,
							 ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::size)),
			 ...);
			return total;
		}
		else
		{
			return SIZE_MAX;
		}
	}

	inline static constexpr ::std::size_t size{static_size()};
};

template <::std::integral char_type, typename T1, typename T2>
struct print_semantic_static_precise_size_impl<char_type, ::fast_io::manipulators::condition<T1, T2>>
{
	using first_size = ::fast_io::details::decay::print_semantic_static_precise_size<
		char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T1>>;
	using second_size = ::fast_io::details::decay::print_semantic_static_precise_size<
		char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T2>>;
	inline static constexpr bool available =
		first_size::available && second_size::available && (first_size::size == second_size::size);
	inline static constexpr ::std::size_t size = available ? first_size::size : SIZE_MAX;
};

template <::std::integral char_type, typename T>
inline constexpr bool print_semantic_precise_leaf_size_ok_v =
	::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t> ||
	::fast_io::static_precise_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
	::fast_io::precise_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
	::fast_io::scatter_printable<char_type, ::std::remove_cvref_t<T>> ||
	::fast_io::reserve_scatters_printable<char_type, ::std::remove_cvref_t<T>>;

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok_impl
	: ::std::bool_constant<
		  ::fast_io::details::decay::print_semantic_precise_leaf_size_ok_v<char_type, T>>
{};

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok
	: ::fast_io::details::decay::print_semantic_precise_size_ok_impl<char_type, ::std::remove_cvref_t<T>>
{};

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_precise_size_ok<char_type, T>
{};

template <::std::integral char_type, typename... Args>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::pack_t<Args...>>
	: ::std::bool_constant<
		  (::fast_io::details::decay::print_semantic_precise_size_ok<
			   char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::value &&
		   ...)>
{};

template <::std::integral char_type, typename T1, typename T2>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::condition<T1, T2>>
	: ::std::bool_constant<
		  ::fast_io::details::decay::print_semantic_precise_size_ok<
			  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T1>>::value && ::fast_io::details::decay::print_semantic_precise_size_ok<char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T2>>::value>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::width_t<placement, T>>
	: ::fast_io::details::decay::print_semantic_precise_size_ok<
		  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T,
		  ::std::integral ch_type>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> && ::fast_io::details::decay::print_semantic_precise_size_ok<
			  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::width_runtime_t<T>>
	: ::fast_io::details::decay::print_semantic_precise_size_ok<
		  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, typename T, ::std::integral ch_type>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> && ::fast_io::details::decay::print_semantic_precise_size_ok<
			  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

template <::std::integral char_type, typename T>
struct print_semantic_params_okay : ::std::false_type
{};

template <::std::integral char_type, typename T>
struct print_semantic_params_okay<char_type, ::fast_io::parameter<T>>
	: print_semantic_params_okay<char_type, ::std::remove_cvref_t<T>>
{};

template <::std::integral char_type, typename... Args>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::pack_t<Args...>>
	: ::std::bool_constant<
		  (print_freestanding_decay_param_okay_single<
			   char_type, print_semantic_forwarded_arg_t<char_type, Args>>::value &&
		   ...)>
{};

template <::std::integral char_type, typename T1, typename T2>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::condition<T1, T2>>
	: ::std::bool_constant<
		  print_freestanding_decay_param_okay_single<
			  char_type, print_semantic_forwarded_arg_t<char_type, T1>>::value &&
		  print_freestanding_decay_param_okay_single<
			  char_type, print_semantic_forwarded_arg_t<char_type, T2>>::value>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::width_t<placement, T>>
	: print_freestanding_decay_param_okay_single<char_type, print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T,
		  ::std::integral ch_type>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> &&
		  print_freestanding_decay_param_okay_single<
			  char_type, print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

template <::std::integral char_type, typename T>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::width_runtime_t<T>>
	: print_freestanding_decay_param_okay_single<char_type, print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, typename T, ::std::integral ch_type>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> &&
		  print_freestanding_decay_param_okay_single<
			  char_type, print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

} // namespace details::decay

} // namespace fast_io
