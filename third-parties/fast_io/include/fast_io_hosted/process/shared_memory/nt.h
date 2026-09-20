#pragma once

#include "../ipc/mode.h"

namespace fast_io
{

namespace win32::nt::details
{

using nt_shared_memory_char_type = char16_t;
using nt_shared_memory_internal_tlc_str =
	::fast_io::containers::basic_string<nt_shared_memory_char_type, ::fast_io::native_thread_local_allocator>;

template <typename... Args>
constexpr inline nt_shared_memory_internal_tlc_str concat_nt_shared_memory_internal_tlc_str(Args &&...args)
{
	return ::fast_io::basic_general_concat_checked<false, nt_shared_memory_char_type,
												   nt_shared_memory_internal_tlc_str>(args...);
}

struct nt_shared_memory_state
{
	void *handle{};
	::std::byte *address{};
	::std::size_t bytes{};
};

inline void nt_shared_memory_validate_mode(ipc_mode mode)
{
	if ((mode & (ipc_mode::sync | ipc_mode::no_block | ipc_mode::message | ipc_mode::huge_page_1G)) !=
		ipc_mode::none) [[unlikely]]
	{
		throw_nt_error((mode & ipc_mode::huge_page_1G) != ipc_mode::none ? 0xC00000BBu : 0xC000000Du);
	}
}

inline ::std::uint_least32_t nt_shared_memory_section_access(ipc_mode mode) noexcept
{
	auto const access_mode{mode & (ipc_mode::in | ipc_mode::out)};
	::std::uint_least32_t access{};
	if (access_mode == ipc_mode::none || (access_mode & ipc_mode::in) != ipc_mode::none)
	{
		access |= 0x00000004u; // SECTION_MAP_READ
	}
	if (access_mode == ipc_mode::none || (access_mode & ipc_mode::out) != ipc_mode::none)
	{
		access |= 0x00000002u; // SECTION_MAP_WRITE
	}
	return access;
}

inline ::std::uint_least32_t nt_shared_memory_page_protection(ipc_mode mode) noexcept
{
	auto const access_mode{mode & (ipc_mode::in | ipc_mode::out)};
	return (access_mode != ipc_mode::none && (access_mode & ipc_mode::out) == ipc_mode::none)
			   ? 0x00000002u  // PAGE_READONLY
			   : 0x00000004u; // PAGE_READWRITE
}

template <nt_family family>
	requires(family == nt_family::nt || family == nt_family::zw)
inline nt_shared_memory_state nt_map_shared_memory_impl(void *handle, ipc_mode mode)
{
	void *address{};
	::std::size_t bytes{};
	void *const current_process{reinterpret_cast<void *>(static_cast<::std::ptrdiff_t>(-1))};
	auto const status{::fast_io::win32::nt::nt_map_view_of_section<family == nt_family::zw>(
		handle, current_process, __builtin_addressof(address), 0u, 0u, nullptr,
		__builtin_addressof(bytes), ::fast_io::win32::nt::section_inherit::ViewShare, 0u,
		nt_shared_memory_page_protection(mode))};
	if (status != 0) [[unlikely]]
	{
		throw_nt_error(status);
	}
	return {handle, reinterpret_cast<::std::byte *>(address), bytes};
}

template <nt_family family>
	requires(family == nt_family::nt || family == nt_family::zw)
inline nt_shared_memory_state nt_family_create_shared_memory_impl(
	::fast_io::win32::nt::object_attributes *attributes, ::std::size_t bytes, ipc_mode mode)
{
	nt_shared_memory_validate_mode(mode);
	if (bytes == 0) [[unlikely]]
	{
		throw_nt_error(0xC000000Du); // STATUS_INVALID_PARAMETER
	}

	::std::uint_least64_t maximum_size{static_cast<::std::uint_least64_t>(bytes)};
	::std::uint_least32_t allocation_attributes{0x08000000u}; // SEC_COMMIT
	if ((mode & ipc_mode::huge_page_2M) != ipc_mode::none)
	{
		allocation_attributes |= 0x80000000u; // SEC_LARGE_PAGES
	}

	void *handle{};
	auto const status{::fast_io::win32::nt::nt_create_section<family == nt_family::zw>(
		__builtin_addressof(handle), nt_shared_memory_section_access(mode), attributes,
		__builtin_addressof(maximum_size), 0x00000004u, allocation_attributes, nullptr)};
	if (status != 0 && status != 0x40000000u) [[unlikely]] // STATUS_OBJECT_NAME_EXISTS
	{
		throw_nt_error(status);
	}

	try
	{
		return nt_map_shared_memory_impl<family>(handle, mode);
	}
	catch (...)
	{
		::fast_io::win32::nt::nt_close<family == nt_family::zw>(handle);
		throw;
	}
}

inline nt_shared_memory_internal_tlc_str nt_shared_memory_path(
	nt_shared_memory_char_type const *name, ::std::size_t name_size)
{
	if (::fast_io::details::is_invalid_dos_filename_with_size(name, name_size)) [[unlikely]]
	{
		throw_nt_error(0xC000003Au); // STATUS_OBJECT_PATH_NOT_FOUND
	}
	auto const session_id{::fast_io::win32::nt::nt_get_current_peb()->SessionId};
	if (session_id == 0)
	{
		return concat_nt_shared_memory_internal_tlc_str(
			u"\\BaseNamedObjects\\fast_io_shared_memory_",
			::fast_io::mnp::os_c_str_with_known_size(name, name_size));
	}
	return concat_nt_shared_memory_internal_tlc_str(
		u"\\Sessions\\", session_id, u"\\BaseNamedObjects\\fast_io_shared_memory_",
		::fast_io::mnp::os_c_str_with_known_size(name, name_size));
}

template <nt_family family>
	requires(family == nt_family::nt || family == nt_family::zw)
inline nt_shared_memory_state nt_family_create_named_shared_memory_impl(
	nt_shared_memory_char_type const *name, ::std::size_t name_size, ::std::size_t bytes, ipc_mode mode)
{
	auto object_name{nt_shared_memory_path(name, name_size)};
	auto const object_name_bytes{
		::fast_io::win32::nt::details::strlen_to_nt_filename_bytes(object_name.size())};
	::fast_io::win32::nt::unicode_string unicode_name{
		.Length = object_name_bytes,
		.MaximumLength = object_name_bytes,
		.Buffer = object_name.data()};
	::fast_io::win32::nt::object_attributes attributes{
		.Length = sizeof(::fast_io::win32::nt::object_attributes),
		.RootDirectory = nullptr,
		.ObjectName = __builtin_addressof(unicode_name),
		.Attributes = 0x000000C0u, // OBJ_CASE_INSENSITIVE | OBJ_OPENIF
		.SecurityDescriptor = nullptr,
		.SecurityQualityOfService = nullptr};
	return nt_family_create_shared_memory_impl<family>(__builtin_addressof(attributes), bytes, mode);
}

template <nt_family family>
struct nt_family_create_named_shared_memory_parameter
{
	::std::size_t bytes{};
	ipc_mode mode{};
	inline nt_shared_memory_state operator()(char16_t const *name, ::std::size_t name_size) const
	{
		return nt_family_create_named_shared_memory_impl<family>(name, name_size, bytes, mode);
	}
};

template <nt_family family, typename T>
	requires((family == nt_family::nt || family == nt_family::zw) &&
			 ::fast_io::constructible_to_os_c_str<T>)
inline nt_shared_memory_state nt_create_named_shared_memory_impl(
	T const &name, ::std::size_t bytes, ipc_mode mode)
{
	return ::fast_io::nt_api_common(
		name, nt_family_create_named_shared_memory_parameter<family>{bytes, mode});
}

template <nt_family family>
	requires(family == nt_family::nt || family == nt_family::zw)
inline nt_shared_memory_state nt_duplicate_shared_memory_impl(void *handle, ipc_mode mode)
{
	auto const duplicated_handle{::fast_io::win32::nt::details::nt_dup_impl<family == nt_family::zw>(handle)};
	try
	{
		return nt_map_shared_memory_impl<family>(duplicated_handle, mode);
	}
	catch (...)
	{
		::fast_io::win32::nt::nt_close<family == nt_family::zw>(duplicated_handle);
		throw;
	}
}

} // namespace win32::nt::details

template <nt_family family>
	requires(family == nt_family::nt || family == nt_family::zw)
class basic_nt_family_shared_memory
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

	inline constexpr basic_nt_family_shared_memory() noexcept = default;

	inline explicit basic_nt_family_shared_memory(
		size_type size, ipc_mode mode = ipc_mode::in | ipc_mode::out)
	{
		::fast_io::win32::nt::object_attributes attributes{
			.Length = sizeof(::fast_io::win32::nt::object_attributes),
			.RootDirectory = nullptr,
			.ObjectName = nullptr,
			.Attributes = 0u,
			.SecurityDescriptor = nullptr,
			.SecurityQualityOfService = nullptr};
		assign(::fast_io::win32::nt::details::nt_family_create_shared_memory_impl<family>(
				   __builtin_addressof(attributes), size, mode),
			   mode);
	}

	template <::fast_io::constructible_to_os_c_str T>
	inline basic_nt_family_shared_memory(
		T const &name, size_type size, ipc_mode mode = ipc_mode::in | ipc_mode::out)
	{
		assign(::fast_io::win32::nt::details::nt_create_named_shared_memory_impl<family>(
				   name, size, mode),
			   mode);
	}

	inline explicit basic_nt_family_shared_memory(
		native_handle_type section_handle, ipc_mode mode = ipc_mode::in | ipc_mode::out)
	{
		::fast_io::win32::nt::details::nt_shared_memory_validate_mode(mode);
		try
		{
			assign(::fast_io::win32::nt::details::nt_map_shared_memory_impl<family>(section_handle, mode), mode);
		}
		catch (...)
		{
			if (section_handle != nullptr) [[likely]]
			{
				::fast_io::win32::nt::nt_close<family == nt_family::zw>(section_handle);
			}
			throw;
		}
	}

	inline basic_nt_family_shared_memory(basic_nt_family_shared_memory const &other)
		: mode{other.mode}
	{
		if (other.handle != nullptr) [[likely]]
		{
			assign(::fast_io::win32::nt::details::nt_duplicate_shared_memory_impl<family>(
					   other.handle, other.mode),
				   other.mode);
		}
	}

	inline basic_nt_family_shared_memory &operator=(basic_nt_family_shared_memory const &other)
	{
		if (__builtin_addressof(other) == this) [[unlikely]]
		{
			return *this;
		}
		basic_nt_family_shared_memory temp{other};
		swap(temp);
		return *this;
	}

	inline basic_nt_family_shared_memory(basic_nt_family_shared_memory &&other) noexcept
		: handle{other.handle}, address{other.address}, bytes{other.bytes}, mode{other.mode}
	{
		other.handle = nullptr;
		other.address = nullptr;
		other.bytes = 0;
	}

	inline basic_nt_family_shared_memory &operator=(basic_nt_family_shared_memory &&other) noexcept
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

	inline void swap(basic_nt_family_shared_memory &other) noexcept
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
			void *const current_process{reinterpret_cast<void *>(static_cast<::std::ptrdiff_t>(-1))};
			::fast_io::win32::nt::nt_unmap_view_of_section<family == nt_family::zw>(current_process, address);
		}
		if (handle != nullptr) [[likely]]
		{
			::fast_io::win32::nt::nt_close<family == nt_family::zw>(handle);
		}
		handle = nullptr;
		address = nullptr;
		bytes = 0;
	}

	inline void close()
	{
		::std::uint_least32_t status{};
		if (address != nullptr) [[likely]]
		{
			void *const current_process{reinterpret_cast<void *>(static_cast<::std::ptrdiff_t>(-1))};
			status = ::fast_io::win32::nt::nt_unmap_view_of_section<family == nt_family::zw>(
				current_process, address);
		}
		if (handle != nullptr) [[likely]]
		{
			auto const close_status{::fast_io::win32::nt::nt_close<family == nt_family::zw>(handle)};
			if (status == 0)
			{
				status = close_status;
			}
		}
		handle = nullptr;
		address = nullptr;
		bytes = 0;
		if (status != 0) [[unlikely]]
		{
			throw_nt_error(status);
		}
	}

	inline ~basic_nt_family_shared_memory()
	{
		reset();
	}

private:
	ipc_mode mode{ipc_mode::in | ipc_mode::out};

	inline void assign(::fast_io::win32::nt::details::nt_shared_memory_state state, ipc_mode new_mode) noexcept
	{
		handle = state.handle;
		address = state.address;
		bytes = state.bytes;
		mode = new_mode;
	}
};

using nt_shared_memory = basic_nt_family_shared_memory<nt_family::nt>;
using zw_shared_memory = basic_nt_family_shared_memory<nt_family::zw>;

namespace freestanding
{

template <nt_family family>
	requires(family == nt_family::nt || family == nt_family::zw)
struct is_trivially_copyable_or_relocatable<basic_nt_family_shared_memory<family>>
{
	inline static constexpr bool value = true;
};

template <nt_family family>
	requires(family == nt_family::nt || family == nt_family::zw)
struct is_zero_default_constructible<basic_nt_family_shared_memory<family>>
{
	inline static constexpr bool value = true;
};

} // namespace freestanding

} // namespace fast_io
