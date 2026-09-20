#pragma once

#include <cstddef>
#include <concepts>
#include <type_traits>
#include <utility>

namespace fast_io::containers
{

namespace details
{

#if __cpp_pack_indexing < 202311L && \
	!(defined(__clang__) || \
	  (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 14))

/// @brief Associates one source-pack position with its exact element type.
/// @details The index is part of the base identity, so repeated element types remain distinct and reference or cv
///          qualification is preserved without constructing an object.
template <::std::size_t index, typename T>
struct pack_indexing_leaf_
{
	static ::std::type_identity<T> select(
		::std::integral_constant<::std::size_t, index>) noexcept;
};

template <typename index_sequence, typename... Args>
struct pack_indexing_table_;

/// @brief Builds one reusable overload table for a complete type pack on compilers without direct pack indexing.
/// @details Every base contributes exactly one overload keyed by its index. Overload resolution selects the requested
///          type without recursively instantiating every preceding suffix. All `I` queries for the same `Args...` reuse
///          this specialization, changing the C++20 fallback from a triangular chain of intermediate classes to one
///          linear table while denoting exactly the same pack element.
template <::std::size_t... index, typename... Args>
	requires(sizeof...(index) == sizeof...(Args))
struct pack_indexing_table_<::std::index_sequence<index...>, Args...>
	: pack_indexing_leaf_<index, Args>...
{
	using pack_indexing_leaf_<index, Args>::select...;
};

template <::std::size_t I, typename... Args>
	requires(I < sizeof...(Args))
struct pack_indexing_before_cxx26_
{
	using table_type = pack_indexing_table_<
		::std::make_index_sequence<sizeof...(Args)>, Args...>;
	using type = typename decltype(table_type::select(
		::std::integral_constant<::std::size_t, I>{}))::type;
};

#endif // no language pack indexing and no compiler type-pack builtin

template <::std::size_t I, typename... Args>
struct pack_indexing_
{
#if __cpp_pack_indexing >= 202311L
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++26-extensions"
#endif
	using type = Args...[I];
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#elif defined(__clang__) || \
	(defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 14)
	// The compiler builtin denotes exactly the same Ith type as language pack indexing, but it is also available in
	// C++20 mode on Clang and newer GCC. Using it avoids the linear chain of intermediate class specializations without
	// changing tuple layout, overload resolution, expression category, or generated code. Other compilers use the
	// portable indexed overload table above.
	using type = __type_pack_element<I, Args...>;
#else  // language pack indexing and the compiler builtin are both unavailable
	using type = typename pack_indexing_before_cxx26_<I, Args...>::type;
#endif
};

template <::std::size_t I, typename... Args>
	requires(I < sizeof...(Args))
using pack_indexing_t_ = typename pack_indexing_<I, Args...>::type;

template <::std::size_t I, typename T>
struct tuple_element_impl_
{
#ifndef __INTELLISENSE__
// GCC 16.1 ICEs in `init_subob_ctx` when an aggregate returned from a function inherits a base containing a
// `[[no_unique_address]]` empty member. The minimized failure is independent of fast_io and GCC 17 trunk accepts the
// original layout, so suppress the attribute only for real GCC 16 and retain the future layout policy unchanged.
#if !(defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 16)
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
#endif
	T val_;
};

template <typename T>
struct pass_type_
{
	using type = T;
};

template <typename IndexSequence, typename... Args>
struct tuple_impl_;

template <::std::size_t... Index, typename... Args>
	requires(sizeof...(Args) == sizeof...(Index))
struct tuple_impl_<::std::index_sequence<Index...>, Args...> : tuple_element_impl_<Index, Args>...
{};

template <typename... Args, ::std::size_t... Index>
	requires(sizeof...(Args) == sizeof...(Index))
[[nodiscard]]
constexpr auto get_tuple_impl_(::std::index_sequence<Index...>) noexcept
{
	return ::fast_io::containers::details::pass_type_<
		::fast_io::containers::details::tuple_impl_<::std::index_sequence<Index...>, Args...>>();
}

} // namespace details

template <typename... Args>
struct tuple : ::fast_io::containers::details::tuple_impl_<::std::make_index_sequence<sizeof...(Args)>, Args...>
{};

template <>
struct tuple<>
{};

template <typename... Args>
tuple(Args &&...) -> tuple<Args...>;

// ADL get
template <::std::size_t I, typename... Args>
FAST_IO_GNU_ALWAYS_INLINE
	[[nodiscard]]
constexpr auto&& get(::fast_io::containers::tuple<Args...> &self) noexcept
{
	// Keep the first conversion on tuple's serialized direct base. Clang can lose a transitive base path when a
	// specialization with module-owned element types crosses a BMI boundary, while the two direct conversions remain
	// well-formed and preserve the identical object representation.
	using tuple_impl_type = ::fast_io::containers::details::tuple_impl_<::std::make_index_sequence<sizeof...(Args)>, Args...>;
	auto& impl{static_cast<tuple_impl_type&>(self)};
	return static_cast<::fast_io::containers::details::tuple_element_impl_<I, ::fast_io::containers::details::pack_indexing_t_<I, Args...>> &>(impl).val_;
}

template <::std::size_t I, typename... Args>
FAST_IO_GNU_ALWAYS_INLINE
	[[nodiscard]]
constexpr auto&& get(::fast_io::containers::tuple<Args...> const &self) noexcept
{
	using tuple_impl_type = ::fast_io::containers::details::tuple_impl_<::std::make_index_sequence<sizeof...(Args)>, Args...>;
	auto const& impl{static_cast<tuple_impl_type const&>(self)};
	return static_cast<::fast_io::containers::details::tuple_element_impl_<I, ::fast_io::containers::details::pack_indexing_t_<I, Args...>> const &>(impl).val_;
}

template <::std::size_t I, typename... Args>
FAST_IO_GNU_ALWAYS_INLINE
	[[nodiscard]]
constexpr auto&& get(::fast_io::containers::tuple<Args...> &&self) noexcept
{
	using tuple_impl_type = ::fast_io::containers::details::tuple_impl_<::std::make_index_sequence<sizeof...(Args)>, Args...>;
	auto&& impl{static_cast<tuple_impl_type&&>(self)};
	return ::std::move(
		static_cast<::fast_io::containers::details::tuple_element_impl_<I, ::fast_io::containers::details::pack_indexing_t_<I, Args...>> &&>(impl).val_);
}

template <::std::size_t I, typename... Args>
FAST_IO_GNU_ALWAYS_INLINE
	[[nodiscard]]
constexpr auto&& get(::fast_io::containers::tuple<Args...> const &&self) noexcept
{
	using tuple_impl_type = ::fast_io::containers::details::tuple_impl_<::std::make_index_sequence<sizeof...(Args)>, Args...>;
	auto const&& impl{static_cast<tuple_impl_type const&&>(self)};
	return ::std::move(
		static_cast<::fast_io::containers::details::tuple_element_impl_<I, ::fast_io::containers::details::pack_indexing_t_<I, Args...>> const &&>(impl).val_);
}

namespace details
{

template <typename T, ::std::size_t I, typename Current, typename... Args>
constexpr auto get_tuple_element_by_type_() noexcept
{
	if constexpr (::std::same_as<T, Current>)
	{
		return ::fast_io::containers::details::pass_type_<tuple_element_impl_<I, Current>>{};
	}
	else
	{
		return ::fast_io::containers::details::get_tuple_element_by_type_<T, I + 1, Args...>();
	}
}

} // namespace details

template <typename T, typename... Args>
	requires((::std::same_as<T, Args> + ...) == 1)
FAST_IO_GNU_ALWAYS_INLINE
	[[nodiscard]]
constexpr auto&& get(::fast_io::containers::tuple<Args...> const &self) noexcept
{
	using tuple_impl_type = ::fast_io::containers::details::tuple_impl_<::std::make_index_sequence<sizeof...(Args)>, Args...>;
	auto const& impl{static_cast<tuple_impl_type const&>(self)};
	return static_cast<typename decltype(::fast_io::containers::details::get_tuple_element_by_type_<T, 0, Args...>())::type const &>(impl).val_;
}

template <typename T, typename... Args>
	requires((::std::same_as<T, Args> + ...) == 1)
FAST_IO_GNU_ALWAYS_INLINE
	[[nodiscard]]
constexpr auto&& get(::fast_io::containers::tuple<Args...> const &&self) noexcept
{
	using tuple_impl_type = ::fast_io::containers::details::tuple_impl_<::std::make_index_sequence<sizeof...(Args)>, Args...>;
	auto const&& impl{static_cast<tuple_impl_type const&&>(self)};
	return ::std::move(
		static_cast<typename decltype(::fast_io::containers::details::get_tuple_element_by_type_<T, 0, Args...>())::type const &&>(impl).val_);
}

template <typename T, typename... Args>
	requires((::std::same_as<T, Args> + ...) == 1)
FAST_IO_GNU_ALWAYS_INLINE
	[[nodiscard]]
constexpr auto&& get(::fast_io::containers::tuple<Args...> &self) noexcept
{
	using tuple_impl_type = ::fast_io::containers::details::tuple_impl_<::std::make_index_sequence<sizeof...(Args)>, Args...>;
	auto& impl{static_cast<tuple_impl_type&>(self)};
	return static_cast<typename decltype(::fast_io::containers::details::get_tuple_element_by_type_<T, 0, Args...>())::type const &>(impl).val_;
}

template <typename T, typename... Args>
	requires((::std::same_as<T, Args> + ...) == 1)
FAST_IO_GNU_ALWAYS_INLINE
	[[nodiscard]]
constexpr auto&& get(::fast_io::containers::tuple<Args...> &&self) noexcept
{
	using tuple_impl_type = ::fast_io::containers::details::tuple_impl_<::std::make_index_sequence<sizeof...(Args)>, Args...>;
	auto&& impl{static_cast<tuple_impl_type&&>(self)};
	return ::std::move(
		static_cast<typename decltype(::fast_io::containers::details::get_tuple_element_by_type_<T, 0, Args...>())::type const &&>(impl).val_);
}

namespace details
{
template <typename F, typename Tuple, std::size_t... I>
inline constexpr decltype(auto) apply_impl(F &&f, Tuple &&t, ::std::index_sequence<I...>)
{
	return ::std::forward<F>(f)(get<I>(::std::forward<Tuple>(t))...);
}

template <typename... Args>
inline consteval ::std::size_t tuple_size(::fast_io::containers::tuple<Args...> const &) noexcept
{
	return sizeof...(Args);
}

} // namespace details

template <typename F, typename Tuple>
inline constexpr decltype(auto) apply(F &&f, Tuple &&t)
{
	constexpr ::std::size_t N{details::tuple_size(t)};
	return details::apply_impl(
		::std::forward<F>(f),
		::std::forward<Tuple>(t),
		::std::make_index_sequence<N>{});
}

namespace details
{

template <typename T>
constexpr bool is_tuple_ = false;

template <typename... Args>
constexpr bool is_tuple_<tuple<Args...>> = true;

} // namespace details

template <typename T>
concept is_tuple = ::fast_io::containers::details::is_tuple_<::std::remove_cvref_t<T>>;

template <typename... Args>
[[nodiscard]]
constexpr auto forward_as_tuple(Args &&...args)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
	return ::fast_io::containers::tuple<Args &&...>{::std::forward<Args>(args)...};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

} // namespace fast_io::containers

template <::std::size_t I, typename... Args>
struct std::tuple_element<I, ::fast_io::containers::tuple<Args...>>
{
	using type = ::fast_io::containers::details::pack_indexing_t_<I, Args...>;
};

template <typename... Args>
struct std::tuple_size<::fast_io::containers::tuple<Args...>>
{
	static constexpr ::std::size_t value = sizeof...(Args);
};
