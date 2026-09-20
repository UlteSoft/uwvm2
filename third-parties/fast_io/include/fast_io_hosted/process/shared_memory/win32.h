#pragma once

#include "../ipc/mode.h"

namespace fast_io
{

namespace win32::details
{

template <win32_family family>
using win32_shared_memory_char_type =
	::std::conditional_t<family == win32_family::wide_nt, char16_t, char>;

template <win32_family family>
using win32_shared_memory_internal_char_type =
	::std::conditional_t<family == win32_family::wide_nt, char16_t, char8_t>;

template <win32_family family>
using win32_shared_memory_internal_tlc_str = ::fast_io::containers::basic_string<
	win32_shared_memory_internal_char_type<family>, ::fast_io::native_thread_local_allocator>;

template <win32_family family, typename... Args>
constexpr inline win32_shared_memory_internal_tlc_str<family>
concat_win32_shared_memory_internal_tlc_str(Args &&...args)
{
	return ::fast_io::basic_general_concat_checked<
		false, win32_shared_memory_internal_char_type<family>,
		win32_shared_memory_internal_tlc_str<family>>(args...);
}

struct win32_shared_memory_state
{
	void *handle{};
	::std::byte *address{};
	::std::size_t bytes{};
};

inline void win32_shared_memory_validate_mode(ipc_mode mode)
{
	if ((mode & (ipc_mode::sync | ipc_mode::no_block | ipc_mode::message | ipc_mode::huge_page_1G)) !=
		ipc_mode::none) [[unlikely]]
	{
		throw_win32_error((mode & ipc_mode::huge_page_1G) != ipc_mode::none ? 0x00000032u : 0x00000057u);
	}
}

inline ::std::uint_least32_t win32_shared_memory_view_access(ipc_mode mode) noexcept
{
	auto const access_mode{mode & (ipc_mode::in | ipc_mode::out)};
	if (access_mode == ipc_mode::none)
	{
		return 0x00000006u; // FILE_MAP_READ | FILE_MAP_WRITE
	}

	::std::uint_least32_t access{};
	if ((access_mode & ipc_mode::in) != ipc_mode::none)
	{
		access |= 0x00000004u; // FILE_MAP_READ
	}
	if ((access_mode & ipc_mode::out) != ipc_mode::none)
	{
		access |= 0x00000002u; // FILE_MAP_WRITE
	}
	if ((mode & ipc_mode::huge_page_2M) != ipc_mode::none)
	{
		access |= 0x20000000u; // FILE_MAP_LARGE_PAGES
	}
	return access;
}

inline win32_shared_memory_state win32_map_shared_memory_impl(
	void *handle, ::std::size_t bytes, ipc_mode mode)
{
	if (bytes == 0) [[unlikely]]
	{
		throw_win32_error(0x00000057u); // ERROR_INVALID_PARAMETER
	}

	void *address{::fast_io::win32::MapViewOfFile(
		handle, win32_shared_memory_view_access(mode), 0u, 0u, bytes)};
	if (address == nullptr) [[unlikely]]
	{
		throw_win32_error();
	}
	return {handle, reinterpret_cast<::std::byte *>(address), bytes};
}

template <win32_family family>
inline win32_shared_memory_state win32_family_create_shared_memory_impl(
	win32_shared_memory_char_type<family> const *name, ::std::size_t bytes, ipc_mode mode)
{
	win32_shared_memory_validate_mode(mode);
	if (bytes == 0) [[unlikely]]
	{
		throw_win32_error(0x00000057u); // ERROR_INVALID_PARAMETER
	}

	auto const mapping_size{static_cast<::std::uint_least64_t>(bytes)};
	::std::uint_least32_t page_protection{0x00000004u | 0x08000000u}; // PAGE_READWRITE | SEC_COMMIT
	if ((mode & ipc_mode::huge_page_2M) != ipc_mode::none)
	{
		page_protection |= 0x80000000u; // SEC_LARGE_PAGES
	}

	void *const invalid_handle{reinterpret_cast<void *>(static_cast<::std::ptrdiff_t>(-1))};
	void *handle{};
	if constexpr (family == win32_family::wide_nt)
	{
		handle = ::fast_io::win32::CreateFileMappingW(
			invalid_handle, nullptr, page_protection,
			static_cast<::std::uint_least32_t>(mapping_size >> 32u),
			static_cast<::std::uint_least32_t>(mapping_size), name);
	}
	else
	{
		handle = ::fast_io::win32::CreateFileMappingA(
			invalid_handle, nullptr, page_protection,
			static_cast<::std::uint_least32_t>(mapping_size >> 32u),
			static_cast<::std::uint_least32_t>(mapping_size),
			reinterpret_cast<char const *>(name));
	}
	if (handle == nullptr) [[unlikely]]
	{
		throw_win32_error();
	}

	try
	{
		return win32_map_shared_memory_impl(handle, bytes, mode);
	}
	catch (...)
	{
		::fast_io::win32::CloseHandle(handle);
		throw;
	}
}

template <win32_family family>
inline win32_shared_memory_state win32_family_create_named_shared_memory_impl(
	win32_shared_memory_char_type<family> const *name, ::std::size_t name_size,
	::std::size_t bytes, ipc_mode mode)
{
	using internal_char_type = win32_shared_memory_internal_char_type<family>;
	using internal_char_const_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= internal_char_type const *;

	auto const begin{reinterpret_cast<internal_char_const_may_alias_ptr>(name)};
	if (::fast_io::details::is_invalid_dos_filename_with_size(begin, name_size)) [[unlikely]]
	{
		throw_win32_error(3u); // ERROR_PATH_NOT_FOUND
	}

	if constexpr (family == win32_family::wide_nt)
	{
		auto const object_name{concat_win32_shared_memory_internal_tlc_str<family>(
			u"fast_io_shared_memory_",
			::fast_io::mnp::os_c_str_with_known_size(begin, name_size))};
		return win32_family_create_shared_memory_impl<family>(object_name.c_str(), bytes, mode);
	}
	else
	{
		auto const object_name{concat_win32_shared_memory_internal_tlc_str<family>(
			u8"fast_io_shared_memory_",
			::fast_io::mnp::os_c_str_with_known_size(begin, name_size))};
		return win32_family_create_shared_memory_impl<family>(
			reinterpret_cast<win32_shared_memory_char_type<family> const *>(object_name.c_str()), bytes, mode);
	}
}

template <win32_family family>
struct win32_family_create_named_shared_memory_parameter
{
	::std::size_t bytes{};
	ipc_mode mode{};
	inline win32_shared_memory_state operator()(
		win32_shared_memory_char_type<family> const *name, ::std::size_t name_size) const
	{
		return win32_family_create_named_shared_memory_impl<family>(name, name_size, bytes, mode);
	}
};

template <win32_family family, typename T>
	requires(::fast_io::constructible_to_os_c_str<T>)
inline win32_shared_memory_state win32_create_named_shared_memory_impl(
	T const &name, ::std::size_t bytes, ipc_mode mode)
{
	return ::fast_io::win32_family_api_common<family>(
		name, win32_family_create_named_shared_memory_parameter<family>{bytes, mode});
}

inline win32_shared_memory_state win32_duplicate_shared_memory_impl(
	void *handle, ::std::size_t bytes, ipc_mode mode)
{
	auto const duplicated_handle{::fast_io::win32::details::win32_dup_impl(handle)};
	try
	{
		return win32_map_shared_memory_impl(duplicated_handle, bytes, mode);
	}
	catch (...)
	{
		::fast_io::win32::CloseHandle(duplicated_handle);
		throw;
	}
}

} // namespace win32::details

template <win32_family family>
class basic_win32_family_shared_memory
{
public:
	using native_handle_type = void *;
	using value_type = ::std::byte;
	using pointer = value_type *;
	using const_pointer = value_type const *;
	using size_type = ::std::size_t;

	native_handle_type handle{};
	pointer address{};
	size_type bytes{};

	inline constexpr basic_win32_family_shared_memory() noexcept = default;

	inline explicit basic_win32_family_shared_memory(
		size_type size, ipc_mode mode = ipc_mode::in | ipc_mode::out)
	{
		assign(::fast_io::win32::details::win32_family_create_shared_memory_impl<family>(nullptr, size, mode), mode);
	}

	template <::fast_io::constructible_to_os_c_str T>
	inline basic_win32_family_shared_memory(
		T const &name, size_type size, ipc_mode mode = ipc_mode::in | ipc_mode::out)
	{
		assign(::fast_io::win32::details::win32_create_named_shared_memory_impl<family>(name, size, mode), mode);
	}

	inline explicit basic_win32_family_shared_memory(
		native_handle_type section_handle, size_type size, ipc_mode mode = ipc_mode::in | ipc_mode::out)
	{
		::fast_io::win32::details::win32_shared_memory_validate_mode(mode);
		try
		{
			assign(::fast_io::win32::details::win32_map_shared_memory_impl(section_handle, size, mode), mode);
		}
		catch (...)
		{
			if (section_handle != nullptr) [[likely]]
			{
				::fast_io::win32::CloseHandle(section_handle);
			}
			throw;
		}
	}

	inline basic_win32_family_shared_memory(basic_win32_family_shared_memory const &other)
		: mode{other.mode}
	{
		if (other.handle != nullptr) [[likely]]
		{
			assign(::fast_io::win32::details::win32_duplicate_shared_memory_impl(
					   other.handle, other.bytes, other.mode),
				   other.mode);
		}
	}

	inline basic_win32_family_shared_memory &operator=(basic_win32_family_shared_memory const &other)
	{
		if (__builtin_addressof(other) == this) [[unlikely]]
		{
			return *this;
		}
		basic_win32_family_shared_memory temp{other};
		swap(temp);
		return *this;
	}

	inline basic_win32_family_shared_memory(basic_win32_family_shared_memory &&other) noexcept
		: handle{other.handle}, address{other.address}, bytes{other.bytes}, mode{other.mode}
	{
		other.handle = nullptr;
		other.address = nullptr;
		other.bytes = 0;
	}

	inline basic_win32_family_shared_memory &operator=(basic_win32_family_shared_memory &&other) noexcept
	{
		if (__builtin_addressof(other) != this) [[likely]]
		{
			reset();
			handle = other.handle;
			address = other.address;
			bytes = other.bytes;
			mode = other.mode;
			other.handle = nullptr;
			other.address = nullptr;
			other.bytes = 0;
		}
		return *this;
	}

	inline constexpr native_handle_type native_handle() const noexcept
	{
		return handle;
	}

	inline explicit constexpr operator bool() const noexcept
	{
		return handle != nullptr;
	}

	inline constexpr pointer data() const noexcept
	{
		return address;
	}

	inline constexpr size_type size() const noexcept
	{
		return bytes;
	}

	inline constexpr bool empty() const noexcept
	{
		return bytes == 0;
	}

	inline constexpr pointer begin() const noexcept
	{
		return address;
	}

	inline constexpr pointer end() const noexcept
	{
		return address == nullptr ? nullptr : address + bytes;
	}

	inline constexpr value_type &operator[](size_type index) const noexcept
	{
		return address[index];
	}

	inline void swap(basic_win32_family_shared_memory &other) noexcept
	{
		auto const old_handle{handle};
		auto const old_address{address};
		auto const old_bytes{bytes};
		auto const old_mode{mode};
		handle = other.handle;
		address = other.address;
		bytes = other.bytes;
		mode = other.mode;
		other.handle = old_handle;
		other.address = old_address;
		other.bytes = old_bytes;
		other.mode = old_mode;
	}

	inline void reset() noexcept
	{
		if (address != nullptr) [[likely]]
		{
			::fast_io::win32::UnmapViewOfFile(address);
		}
		if (handle != nullptr) [[likely]]
		{
			::fast_io::win32::CloseHandle(handle);
		}
		handle = nullptr;
		address = nullptr;
		bytes = 0;
	}

	inline void close()
	{
		::std::uint_least32_t error{};
		if (address != nullptr) [[likely]]
		{
			if (!::fast_io::win32::UnmapViewOfFile(address))
			{
				error = ::fast_io::win32::GetLastError();
			}
		}
		if (handle != nullptr) [[likely]]
		{
			if (!::fast_io::win32::CloseHandle(handle) && error == 0)
			{
				error = ::fast_io::win32::GetLastError();
			}
		}
		handle = nullptr;
		address = nullptr;
		bytes = 0;
		if (error != 0) [[unlikely]]
		{
			throw_win32_error(error);
		}
	}

	inline ~basic_win32_family_shared_memory()
	{
		reset();
	}

private:
	ipc_mode mode{ipc_mode::in | ipc_mode::out};

	inline void assign(::fast_io::win32::details::win32_shared_memory_state state, ipc_mode new_mode) noexcept
	{
		handle = state.handle;
		address = state.address;
		bytes = state.bytes;
		mode = new_mode;
	}
};

using win32_shared_memory_9xa = basic_win32_family_shared_memory<win32_family::ansi_9x>;
using win32_shared_memory_ntw = basic_win32_family_shared_memory<win32_family::wide_nt>;
using win32_shared_memory = basic_win32_family_shared_memory<win32_family::native>;

namespace freestanding
{

template <win32_family family>
struct is_trivially_copyable_or_relocatable<basic_win32_family_shared_memory<family>>
{
	inline static constexpr bool value = true;
};

template <win32_family family>
struct is_zero_default_constructible<basic_win32_family_shared_memory<family>>
{
	inline static constexpr bool value = true;
};

} // namespace freestanding

} // namespace fast_io
