#pragma once

namespace fast_io
{

namespace manipulators
{
/// @brief Hidden carrier for indentation-normalized multiline text.
/// @details `first`/`last` borrow the source, `container` stores the maximum text width observed before each tab column,
///          and `total_size` is a conservative rectangular reserve bound for the transformed lines.
template <::std::integral char_type, typename containe>
struct auto_indent_t
{
	using container_type = containe;
	::std::size_t total_size;
	char_type const *first;
	char_type const *last;
	container_type container;
};
} // namespace manipulators

namespace details
{

template <::std::integral char_type, ::std::size_t op = 0>
inline constexpr char_type get_horizontal_exec_char() noexcept
{
	if constexpr (op == 2)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return ' ';
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return L' ';
		}
		else
		{
			return u8' ';
		}
	}
	else if constexpr (op == 1)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return '\n';
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return L'\n';
		}
		else
		{
			return u8'\n';
		}
	}
	else
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return '\t';
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return L'\t';
		}
		else
		{
			return u8'\t';
		}
	}
}

template <typename containe, ::std::integral char_type>
inline ::fast_io::manipulators::auto_indent_t<char_type, containe> calculate_auto_width_result(char_type const *first2,
																							   char_type const *last)
{
	auto first{first2};
	containe cont;
	constexpr auto slasht{get_horizontal_exec_char<char_type>()};
	constexpr auto lf{get_horizontal_exec_char<char_type, 1>()};
	::std::size_t pos_this_line{};
	::std::size_t lines{};
	auto last_tab{first};
	for (; first != last; ++first)
	{
		if ((*first == slasht) | (*first == lf))
		{
			::std::size_t this_tab_width{static_cast<::std::size_t>(first - last_tab)};
			if (pos_this_line < cont.size())
			{
				if (cont[pos_this_line] < this_tab_width)
				{
					cont[pos_this_line] = this_tab_width;
				}
			}
			else
			{
				cont.emplace_back(this_tab_width);
			}
			last_tab = first + 1;
			if (*first == lf)
			{
				pos_this_line = 0;
				++lines;
			}
			else
			{
				++pos_this_line;
			}
		}
	}
	if (last_tab != last)
	{
		::std::size_t this_tab_width{static_cast<::std::size_t>(last - last_tab)};
		if (pos_this_line < cont.size())
		{
			if (cont[pos_this_line] < this_tab_width)
			{
				cont[pos_this_line] = this_tab_width;
			}
		}
		else
		{
			cont.emplace_back(this_tab_width);
		}
		++lines;
	}
	::std::size_t total_length{1};
	for (auto e : cont)
	{
		total_length = intrinsics::add_or_overflow_die_chain(total_length, e, static_cast<::std::size_t>(1));
	}
	total_length = intrinsics::mul_or_overflow_die(total_length, lines);
	return {total_length, first2, last, ::std::move(cont)};
}

template <::std::integral char_type, typename containe>
inline constexpr char_type *
print_reserve_define_auto_indent(char_type *iter,
								 ::fast_io::manipulators::auto_indent_t<char_type, containe> const &indent) noexcept
{
	constexpr auto slasht{get_horizontal_exec_char<char_type>()};
	constexpr auto lf{get_horizontal_exec_char<char_type, 1>()};
	constexpr auto space{get_horizontal_exec_char<char_type, 2>()};
	auto first{indent.first};
	auto last{indent.last};
	auto line_ptr{indent.container.data()};
	auto line_size{indent.container.size()};
	auto last_tab{first};
	for (::std::size_t pos_this_line{}; first != last; ++first)
	{
		bool const slt{*first == slasht};
		if ((slt) | (*first == lf)) [[unlikely]]
		{
			::std::size_t diff{static_cast<::std::size_t>(first - last_tab)};
			iter = non_overlapped_copy_n(last_tab, diff, iter);
			last_tab = first + 1;
			if (slt) [[likely]]
			{
				iter = my_fill_n(iter, line_ptr[pos_this_line] - diff, space);
				*iter = slasht;
				++iter;
				++pos_this_line;
			}
			else
			{
				*iter = lf;
				++iter;
				pos_this_line = 0;
			}
		}
	}
	if (last_tab != last)
	{
		iter = non_overlapped_copy(last_tab, first, iter);
	}
	return iter;
}

} // namespace details

namespace manipulators
{

/// @brief Returns the precomputed output reserve bound of an `auto_indent_t` transformation.
/// @details The bound assumes every counted line may occupy every maximum-width tab column and can exceed the number of
///          code units actually emitted; this hidden CPO performs no source traversal.
template <::std::integral char_type, typename containe>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<char_type, auto_indent_t<char_type, containe>>,
												  auto_indent_t<char_type, containe> const &indent) noexcept
{
	return indent.total_size;
}

/// @brief Returns the stack staging size used for dynamic auto-indent output.
/// @details The hidden CPO is type-based and independent of source text.
template <::std::integral char_type, typename containe>
inline constexpr ::std::size_t
print_reserve_static_stack_size(io_reserve_type_t<char_type, auto_indent_t<char_type, containe>>) noexcept
{
	return ::fast_io::details::dynamic_reserve_default_static_stack_size<char_type>();
}

/// @brief Emits indentation-normalized multiline text into contiguous output.
/// @details Before each existing tab, spaces are inserted until that field reaches the maximum width observed for its
///          column, then the original tab is emitted. Line feeds and all non-tab text are preserved; the last field is
///          not padded and no trailing line feed is synthesized. The returned pointer identifies the actual end, which
///          may precede the conservative reserved extent.
template <::std::integral char_type, typename containe>
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, auto_indent_t<char_type, containe>>,
												 char_type *iter,
												 auto_indent_t<char_type, containe> const &indent) noexcept
{
	return ::fast_io::details::print_reserve_define_auto_indent(iter, indent);
}

} // namespace manipulators

} // namespace fast_io
