#pragma once

/*
 * Public IO-level print-source diagnostics.
 *
 * These predicates classify raw source types whose spelling has more than one
 * plausible meaning at a user-facing IO boundary.  Low-level print operations
 * remain policy-free; IO facades call print_raw_static_assert before entering
 * their ordinary source-normalization path.
 */

namespace fast_io
{

namespace details
{

// bool is intentionally not classified here: its ordinary scalar spelling is
// 0/1, while mnp::boolalpha is an explicit alphabetic formatting request.
template <typename T>
inline constexpr bool raw_character_scalar_print_arg{
	::fast_io::details::character_integral<::std::remove_cvref_t<T>>};

template <typename T>
using raw_character_pointer_pointee_t =
	::std::remove_cv_t<::std::remove_pointer_t<::std::remove_cvref_t<T>>>;

template <typename T>
inline constexpr bool raw_character_pointer_print_arg{
	::std::is_pointer_v<::std::remove_cvref_t<T>> &&
	::fast_io::details::character_integral<raw_character_pointer_pointee_t<T>>};

template <typename T>
inline constexpr bool raw_function_pointer_print_arg{
	::std::is_pointer_v<::std::remove_cvref_t<T>> &&
	::std::is_function_v<raw_character_pointer_pointee_t<T>>};

template <typename T>
inline constexpr bool raw_non_character_pointer_print_arg{
	::std::is_pointer_v<::std::remove_cvref_t<T>> && (!raw_character_pointer_print_arg<T>) &&
	(!raw_function_pointer_print_arg<T>)};

template <typename T>
inline constexpr bool raw_member_object_pointer_print_arg{
	::std::is_member_object_pointer_v<::std::remove_cvref_t<T>>};

template <typename T>
inline constexpr bool raw_member_function_pointer_print_arg{
	::std::is_member_function_pointer_v<::std::remove_cvref_t<T>>};

template <typename... Args>
inline constexpr bool has_raw_character_scalar_print_arg{(... || raw_character_scalar_print_arg<Args>)};

template <typename... Args>
inline constexpr bool has_raw_character_pointer_print_arg{(... || raw_character_pointer_print_arg<Args>)};

template <typename... Args>
inline constexpr bool has_raw_function_pointer_print_arg{(... || raw_function_pointer_print_arg<Args>)};

template <typename... Args>
inline constexpr bool has_raw_non_character_pointer_print_arg{(... || raw_non_character_pointer_print_arg<Args>)};

template <typename... Args>
inline constexpr bool has_raw_member_object_pointer_print_arg{(... || raw_member_object_pointer_print_arg<Args>)};

template <typename... Args>
inline constexpr bool has_raw_member_function_pointer_print_arg{(... || raw_member_function_pointer_print_arg<Args>)};

template <typename... Args>
inline constexpr bool has_raw_print_arg{has_raw_character_scalar_print_arg<Args...> ||
										has_raw_character_pointer_print_arg<Args...> ||
										has_raw_function_pointer_print_arg<Args...> ||
										has_raw_non_character_pointer_print_arg<Args...> ||
										has_raw_member_object_pointer_print_arg<Args...> ||
										has_raw_member_function_pointer_print_arg<Args...>};

template <bool has_raw_character_scalar>
inline constexpr void print_raw_character_scalar_static_assert() noexcept
{
	static_assert(!has_raw_character_scalar,
				  "fast_io: raw character scalar is ambiguous. Use mnp::chvw(ch) for character text or "
				  "mnp::dec(ch) for its code value.");
}

template <bool has_raw_character_pointer>
inline constexpr void print_raw_character_pointer_static_assert() noexcept
{
	static_assert(!has_raw_character_pointer,
				  "fast_io: raw character pointer is ambiguous. Use mnp::pointervw(ptr) for pointer value or "
				  "mnp::os_c_str(ptr) for OS/C string text.");
}

template <bool has_raw_non_character_pointer>
inline constexpr void print_raw_non_character_pointer_static_assert() noexcept
{
	static_assert(!has_raw_non_character_pointer,
				  "fast_io: raw pointer is not printable directly. Use mnp::pointervw(ptr) for pointer value.");
}

template <bool has_raw_function_pointer>
inline constexpr void print_raw_function_pointer_static_assert() noexcept
{
	static_assert(!has_raw_function_pointer,
				  "fast_io: raw function pointer is not printable directly. Use mnp::funcvw(fn) for function address.");
}

template <bool has_raw_member_object_pointer>
inline constexpr void print_raw_member_object_pointer_static_assert() noexcept
{
	static_assert(!has_raw_member_object_pointer,
				  "fast_io: raw member object pointer is not printable directly. Use mnp::fieldptrvw(ptr) for its "
				  "member-pointer representation.");
}

template <bool has_raw_member_function_pointer>
inline constexpr void print_raw_member_function_pointer_static_assert() noexcept
{
	static_assert(!has_raw_member_function_pointer,
				  "fast_io: raw member function pointer is not printable directly. Use mnp::methodvw(ptr) for its "
				  "member-pointer representation.");
}

template <typename... Args>
inline constexpr void print_raw_static_assert() noexcept
{
	if constexpr (has_raw_character_scalar_print_arg<Args...>)
	{
		print_raw_character_scalar_static_assert<has_raw_character_scalar_print_arg<Args...>>();
	}
	else if constexpr (has_raw_character_pointer_print_arg<Args...>)
	{
		print_raw_character_pointer_static_assert<has_raw_character_pointer_print_arg<Args...>>();
	}
	else if constexpr (has_raw_function_pointer_print_arg<Args...>)
	{
		print_raw_function_pointer_static_assert<has_raw_function_pointer_print_arg<Args...>>();
	}
	else if constexpr (has_raw_non_character_pointer_print_arg<Args...>)
	{
		print_raw_non_character_pointer_static_assert<has_raw_non_character_pointer_print_arg<Args...>>();
	}
	else if constexpr (has_raw_member_object_pointer_print_arg<Args...>)
	{
		print_raw_member_object_pointer_static_assert<has_raw_member_object_pointer_print_arg<Args...>>();
	}
	else if constexpr (has_raw_member_function_pointer_print_arg<Args...>)
	{
		print_raw_member_function_pointer_static_assert<has_raw_member_function_pointer_print_arg<Args...>>();
	}
}

} // namespace details

} // namespace fast_io
