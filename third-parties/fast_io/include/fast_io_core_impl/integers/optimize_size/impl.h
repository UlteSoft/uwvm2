#pragma once

namespace fast_io::details
{

namespace optimize_size
{

namespace with_length
{
template <char8_t base = 10, bool uppercase = false, ::std::integral char_type, my_unsigned_integral U>
inline constexpr void output_unsigned(char_type *str, U value, ::std::size_t const len) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	str += len;
	for (::std::size_t i{}; i != len; ++i)
	{
		U const temp(value / base);
		char8_t const res{static_cast<char8_t>(value % base)};
		--str;
		if constexpr (base <= 10)
		{
			if constexpr (::fast_io::details::is_ebcdic<char_type>)
			{
				*str = static_cast<char_type>(0xF0 + res);
			}
			else if constexpr (::fast_io::details::is_ascii<char_type>)
			{
				*str = static_cast<char_type>(u8'0' + res);
			}
			else
			{
				*str = ::fast_io::details::charliteralofnumber<char_type, uppercase>(
					static_cast<char8_t>(res));
			}
		}
		else
		{
			if constexpr (::fast_io::details::is_classic_ebcdic<char_type>)
			{
				if (res < 10)
				{
					*str = static_cast<char_type>(0xF0 + res);
				}
				else
				{
					if constexpr (base <= 19)
					{
						if constexpr (uppercase)
						{
							*str = static_cast<char_type>((0xC1 - 10) + res);
						}
						else
						{
							*str = static_cast<char_type>((0x81 - 10) + res);
						}
					}
					else if constexpr (base <= 28)
					{
						if (res < 19)
						{
							if constexpr (uppercase)
							{
								*str = static_cast<char_type>((0xC1 - 10) + res);
							}
							else
							{
								*str = static_cast<char_type>((0x81 - 10) + res);
							}
						}
						else
						{
							if constexpr (uppercase)
							{
								*str = static_cast<char_type>((0xD1 - 19) + res);
							}
							else
							{
								*str = static_cast<char_type>((0x91 - 19) + res);
							}
						}
					}
					else
					{
						if (res < 19)
						{
							if constexpr (uppercase)
							{
								*str = static_cast<char_type>((0xC1 - 10) + res);
							}
							else
							{
								*str = static_cast<char_type>((0x81 - 10) + res);
							}
						}
						else if (res < 28)
						{
							if constexpr (uppercase)
							{
								*str = static_cast<char_type>((0xD1 - 19) + res);
							}
							else
							{
								*str = static_cast<char_type>((0x91 - 19) + res);
							}
						}
						else
						{
							if constexpr (uppercase)
							{
								*str = static_cast<char_type>((0xE2 - 28) + res);
							}
							else
							{
								*str = static_cast<char_type>((0xA2 - 28) + res);
							}
						}
					}
				}
			}
			else if constexpr (::fast_io::details::is_ascii<char_type>)
			{
				if (res < 10)
				{
					*str = static_cast<char_type>(u8'0' + res);
				}
				else
				{
					if constexpr (uppercase)
					{
						*str = static_cast<char_type>((u8'A' - 10) + res);
					}
					else
					{
						*str = static_cast<char_type>((u8'a' - 10) + res);
					}
				}
			}
			else
			{
				*str = ::fast_io::details::charliteralofnumber<char_type, uppercase>(
					static_cast<char8_t>(res));
			}
		}
		if constexpr (::std::same_as<char_type, wchar_t> &&
					  ::fast_io::details::wide_is_none_ebcdic_endian &&
					  ::fast_io::details::is_ebcdic<char_type> &&
					  (base <= 10 || ::fast_io::details::is_classic_ebcdic<char_type>))
		{
			*str = static_cast<char_type>(::fast_io::byte_swap(static_cast<unsigned_char_type>(*str)));
		}
		value = temp;
	}
}
} // namespace with_length

template <::std::size_t len, char8_t base = 10, bool uppercase = false, ::std::integral char_type,
		  my_unsigned_integral U>
inline constexpr void output_unsigned_dummy(char_type *str, U value) noexcept
{
	if constexpr (sizeof(U) <= sizeof(unsigned))
	{
		with_length::output_unsigned<base, uppercase>(str, static_cast<unsigned>(value), len);
	}
	else
	{
		with_length::output_unsigned<base, uppercase>(str, value, len);
	}
}

template <char8_t base = 10, bool uppercase = false, ::std::integral char_type, my_unsigned_integral U>
inline constexpr ::std::size_t output_unsigned(char_type *str, U value) noexcept
{
	::std::size_t const len{chars_len<base>(value)};
	if constexpr (sizeof(U) <= sizeof(unsigned))
	{
		with_length::output_unsigned(str, static_cast<unsigned>(value), len);
	}
	else
	{
		with_length::output_unsigned(str, value, len);
	}
	return len;
}

template <char8_t base = 10, bool uppercase = false, ::std::integral char_type, my_unsigned_integral uint_type>
inline constexpr char_type *output_unsigned_full(char_type *str, uint_type value) noexcept
{
	constexpr ::std::size_t len{cal_max_int_size<uint_type, base>()};
	if constexpr (sizeof(uint_type) <= sizeof(unsigned))
	{
		with_length::output_unsigned<base, uppercase>(str, static_cast<unsigned>(value), len);
	}
	else
	{
		with_length::output_unsigned<base, uppercase>(str, value, len);
	}
	return str + len;
}

} // namespace optimize_size

} // namespace fast_io::details
