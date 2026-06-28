#pragma once

namespace fast_io
{

namespace win32::nt::details
{

struct nt_file_loader_return_value_t
{
	char *address_begin{};
	char *address_end{};
	char *address_capacity{};
	::std::size_t file_mapping_size{};
	::std::size_t anonymous_mapping_size{};
};

inline ::std::size_t nt_file_loader_add_extra_bytes(::std::size_t file_size, ::std::size_t extra_bytes)
{
	if (SIZE_MAX - file_size < extra_bytes)
	{
		throw_nt_error(0xC0000095); // STATUS_INTEGER_OVERFLOW
	}
	return file_size + extra_bytes;
}

inline ::std::size_t nt_file_loader_round_up_to_page(::std::size_t value, ::std::size_t page_size)
{
	if (value == 0)
	{
		return 0;
	}
	::std::size_t const rem{value % page_size};
	if (rem == 0)
	{
		return value;
	}
	::std::size_t const inc{page_size - rem};
	if (SIZE_MAX - value < inc)
	{
		throw_nt_error(0xC0000095); // STATUS_INTEGER_OVERFLOW
	}
	return value + inc;
}

inline bool nt_file_loader_is_private_write(::fast_io::nt_mmap_options const &options) noexcept
{
	return options.flProtect == 0x08u /*PAGE_WRITECOPY*/ ||
		   options.flProtect == 0x80u /*PAGE_EXECUTE_WRITECOPY*/;
}

inline ::std::uint_least32_t nt_file_loader_writecopy_protect(::std::uint_least32_t flprotect) noexcept
{
	if (flprotect & (0x10u | 0x20u | 0x40u | 0x80u))
	{
		return 0x80u /*PAGE_EXECUTE_WRITECOPY*/;
	}
	return 0x08u /*PAGE_WRITECOPY*/;
}

inline ::std::uint_least32_t nt_file_loader_file_protect(::fast_io::nt_mmap_options const &options) noexcept
{
	if (nt_file_loader_is_private_write(options))
	{
		return nt_file_loader_writecopy_protect(options.flProtect);
	}
	return options.flProtect;
}

inline ::std::uint_least32_t nt_file_loader_section_access(::fast_io::nt_mmap_options const &options) noexcept
{
	if (nt_file_loader_is_private_write(options))
	{
		return (options.dwDesiredAccess | 0x0004u /*SECTION_MAP_READ*/) & ~0x0002u /*SECTION_MAP_WRITE*/;
	}
	return options.dwDesiredAccess;
}

inline ::std::uint_least32_t nt_file_loader_anonymous_protect(::std::uint_least32_t flprotect) noexcept
{
	if (flprotect == 0x08u /*PAGE_WRITECOPY*/)
	{
		return 0x04u /*PAGE_READWRITE*/;
	}
	if (flprotect == 0x80u /*PAGE_EXECUTE_WRITECOPY*/)
	{
		return 0x40u /*PAGE_EXECUTE_READWRITE*/;
	}
	return flprotect;
}

template <::fast_io::nt_family family>
inline ::std::size_t nt_file_loader_get_page_size()
{
	::fast_io::win32::nt::system_basic_information sb{};
	auto status{::fast_io::win32::nt::nt_query_system_information<family == ::fast_io::nt_family::zw>(
		::fast_io::win32::nt::system_information_class::SystemBasicInformation, __builtin_addressof(sb),
		static_cast<::std::uint_least32_t>(sizeof(sb)), nullptr)};
	if (status)
	{
		throw_nt_error(status);
	}
	if (sb.PageSize != 0)
	{
		return sb.PageSize;
	}
	return 4096u;
}

inline constexpr void *nt_file_loader_current_process() noexcept
{
	return reinterpret_cast<void *>(static_cast<::std::ptrdiff_t>(-1));
}

template <::fast_io::nt_family family>
inline void nt_free_virtual_memory_release(void *address) noexcept
{
	if (address)
	{
		void *base_address{address};
		::std::size_t region_size{};
		::fast_io::win32::nt::nt_free_virtual_memory<family == ::fast_io::nt_family::zw>(
			nt_file_loader_current_process(), __builtin_addressof(base_address), __builtin_addressof(region_size),
			0x00008000u /*MEM_RELEASE*/);
	}
}

template <::fast_io::nt_family family>
inline void nt_unload_address(void *address, ::std::size_t file_mapping_size = 0,
							  ::std::size_t anonymous_mapping_size = 0) noexcept
{
	if (address)
	{
		if (file_mapping_size || anonymous_mapping_size)
		{
			if (file_mapping_size)
			{
				::fast_io::win32::nt::nt_unmap_view_of_section<family == ::fast_io::nt_family::zw>(
					nt_file_loader_current_process(), address);
			}
			if (anonymous_mapping_size)
			{
				nt_free_virtual_memory_release<family>(reinterpret_cast<char *>(address) + file_mapping_size);
			}
		}
		else
		{
			::fast_io::win32::nt::nt_unmap_view_of_section<family == ::fast_io::nt_family::zw>(
				nt_file_loader_current_process(), address);
		}
	}
}

template <::fast_io::nt_family family>
struct nt_file_loader_composite_guard
{
	char *address{};
	::std::size_t prefix_placeholder_size{};
	::std::size_t tail_placeholder_offset{};
	::std::size_t tail_placeholder_size{};
	::std::size_t file_mapping_size{};
	::std::size_t anonymous_mapping_size{};
	inline explicit constexpr nt_file_loader_composite_guard() noexcept = default;
	inline explicit constexpr nt_file_loader_composite_guard(char *addr, ::std::size_t n) noexcept
		: address{addr}, prefix_placeholder_size{n}
	{}
	inline nt_file_loader_composite_guard(nt_file_loader_composite_guard const &) = delete;
	inline nt_file_loader_composite_guard &operator=(nt_file_loader_composite_guard const &) = delete;
	inline constexpr void split_tail_placeholder(::std::size_t prefix, ::std::size_t tail) noexcept
	{
		prefix_placeholder_size = prefix;
		tail_placeholder_offset = prefix;
		tail_placeholder_size = tail;
	}
	inline constexpr void mark_file_mapped(::std::size_t n) noexcept
	{
		prefix_placeholder_size = 0;
		file_mapping_size = n;
	}
	inline constexpr void mark_prefix_placeholder_released() noexcept
	{
		prefix_placeholder_size = 0;
	}
	inline constexpr void mark_anonymous_mapped(::std::size_t n) noexcept
	{
		if (tail_placeholder_offset == 0)
		{
			prefix_placeholder_size = 0;
		}
		tail_placeholder_size = 0;
		anonymous_mapping_size = n;
	}
	inline constexpr void release() noexcept
	{
		address = nullptr;
		prefix_placeholder_size = 0;
		tail_placeholder_offset = 0;
		tail_placeholder_size = 0;
		file_mapping_size = 0;
		anonymous_mapping_size = 0;
	}
	inline ~nt_file_loader_composite_guard()
	{
		if (address == nullptr)
		{
			return;
		}
		if (file_mapping_size)
		{
			::fast_io::win32::nt::nt_unmap_view_of_section<family == ::fast_io::nt_family::zw>(
				nt_file_loader_current_process(), address);
		}
		if (prefix_placeholder_size)
		{
			nt_free_virtual_memory_release<family>(address);
		}
		auto tail{address + tail_placeholder_offset};
		if (anonymous_mapping_size)
		{
			nt_free_virtual_memory_release<family>(tail);
		}
		if (tail_placeholder_size)
		{
			nt_free_virtual_memory_release<family>(tail);
		}
	}
};

template <::fast_io::nt_family family>
inline nt_file_loader_return_value_t nt_load_address_options_impl(::fast_io::nt_mmap_options const &options,
																  void *handle)
{
	using secattr_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= ::fast_io::win32::nt::object_attributes *;

	::std::size_t const fsz{::fast_io::win32::nt::details::nt_load_file_get_file_size<(family == ::fast_io::nt_family::zw)>(handle)};
	::std::size_t const capacity{nt_file_loader_add_extra_bytes(fsz, options.extra_bytes)};

	if (capacity == 0)
	{
		return {nullptr, nullptr, nullptr};
	}
	if (options.extra_bytes)
	{
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0A00
		throw_nt_error(0xC00000BB); // STATUS_NOT_SUPPORTED
#else
		::std::size_t const page_size{nt_file_loader_get_page_size<family>()};
		::std::size_t const file_mapping_size{nt_file_loader_round_up_to_page(fsz, page_size)};
		::std::size_t const total_mapping_size{nt_file_loader_round_up_to_page(capacity, page_size)};
		::std::size_t const anonymous_mapping_size{total_mapping_size - file_mapping_size};
		::std::uint_least32_t const file_protect{nt_file_loader_file_protect(options)};
		::std::uint_least32_t const section_access{nt_file_loader_section_access(options)};
		::std::uint_least32_t const anonymous_protect{nt_file_loader_anonymous_protect(file_protect)};
		::fast_io::win32::nt::object_attributes objAttr;
		secattr_ptr pobjattr{reinterpret_cast<secattr_ptr>(options.objAttr)};
		if (pobjattr == nullptr)
		{
			objAttr = {};
			objAttr.Length = sizeof(::fast_io::win32::nt::object_attributes);
			pobjattr = __builtin_addressof(objAttr);
		}
		if (capacity <= file_mapping_size)
		{
			void *h_section{};
			auto status{::fast_io::win32::nt::nt_create_section<family == ::fast_io::nt_family::zw>(
				__builtin_addressof(h_section), section_access, pobjattr, nullptr, file_protect,
				options.attributes, handle)};
			if (status)
			{
				throw_nt_error(status);
			}
			::fast_io::basic_nt_family_file<family, char> map_hd{h_section};
			void *p_map_address{};
			::std::size_t view_size{};
			status = ::fast_io::win32::nt::nt_map_view_of_section<family == ::fast_io::nt_family::zw>(
				h_section, nt_file_loader_current_process(), __builtin_addressof(p_map_address), 0u, 0u, nullptr,
				__builtin_addressof(view_size), static_cast<::fast_io::win32::nt::section_inherit>(options.viewShare),
				0u, file_protect);
			if (status)
			{
				throw_nt_error(status);
			}
			auto address{reinterpret_cast<char *>(p_map_address)};
			return {address, address + fsz, address + capacity, file_mapping_size, 0};
		}
		void *base_address{};
		::std::size_t reserve_size{total_mapping_size};
		auto status{::fast_io::win32::nt::nt_allocate_virtual_memory_ex<family == ::fast_io::nt_family::zw>(
			nt_file_loader_current_process(), __builtin_addressof(base_address), __builtin_addressof(reserve_size),
			0x00002000u | 0x00040000u /*MEM_RESERVE | MEM_RESERVE_PLACEHOLDER*/, 0x01u /*PAGE_NOACCESS*/,
			nullptr, 0)};
		if (status)
		{
			throw_nt_error(status);
		}
		auto address{reinterpret_cast<char *>(base_address)};
		nt_file_loader_composite_guard<family> guard{address, total_mapping_size};
		if (file_mapping_size)
		{
			void *tail_placeholder{address + file_mapping_size};
			::std::size_t tail_placeholder_size{anonymous_mapping_size};
			status = ::fast_io::win32::nt::nt_free_virtual_memory<family == ::fast_io::nt_family::zw>(
				nt_file_loader_current_process(), __builtin_addressof(tail_placeholder),
				__builtin_addressof(tail_placeholder_size),
				0x00008000u | 0x00000002u /*MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER*/);
			if (status)
			{
				throw_nt_error(status);
			}
			guard.split_tail_placeholder(file_mapping_size, anonymous_mapping_size);
			void *prefix_placeholder{address};
			::std::size_t prefix_placeholder_size{};
			status = ::fast_io::win32::nt::nt_free_virtual_memory<family == ::fast_io::nt_family::zw>(
				nt_file_loader_current_process(), __builtin_addressof(prefix_placeholder),
				__builtin_addressof(prefix_placeholder_size), 0x00008000u /*MEM_RELEASE*/);
			if (status)
			{
				throw_nt_error(status);
			}
			guard.mark_prefix_placeholder_released();
			void *h_section{};
			status = ::fast_io::win32::nt::nt_create_section<family == ::fast_io::nt_family::zw>(
				__builtin_addressof(h_section), section_access, pobjattr, nullptr, file_protect,
				options.attributes, handle);
			if (status)
			{
				throw_nt_error(status);
			}
			::fast_io::basic_nt_family_file<family, char> map_hd{h_section};
			void *p_map_address{address};
			::std::size_t view_size{};
			status = ::fast_io::win32::nt::nt_map_view_of_section_ex<family == ::fast_io::nt_family::zw>(
				h_section, nt_file_loader_current_process(), __builtin_addressof(p_map_address), nullptr,
				__builtin_addressof(view_size), 0, file_protect, nullptr, 0);
			if (status)
			{
				throw_nt_error(status);
			}
			if (p_map_address != address)
			{
				if (p_map_address != nullptr)
				{
					::fast_io::win32::nt::nt_unmap_view_of_section<family == ::fast_io::nt_family::zw>(
						nt_file_loader_current_process(), p_map_address);
				}
				throw_nt_error(0xC0000018); // STATUS_CONFLICTING_ADDRESSES
			}
			guard.mark_file_mapped(file_mapping_size);
		}
		void *tail_address{address + file_mapping_size};
		::std::size_t tail_region_size{anonymous_mapping_size};
		status = ::fast_io::win32::nt::nt_allocate_virtual_memory_ex<family == ::fast_io::nt_family::zw>(
			nt_file_loader_current_process(), __builtin_addressof(tail_address),
			__builtin_addressof(tail_region_size),
			0x00001000u | 0x00002000u | 0x00004000u /*MEM_COMMIT | MEM_RESERVE | MEM_REPLACE_PLACEHOLDER*/,
			anonymous_protect, nullptr, 0);
		if (status)
		{
			throw_nt_error(status);
		}
		if (tail_address != address + file_mapping_size)
		{
			if (tail_address != nullptr)
			{
				nt_free_virtual_memory_release<family>(tail_address);
			}
			throw_nt_error(0xC0000018); // STATUS_CONFLICTING_ADDRESSES
		}
		guard.mark_anonymous_mapped(anonymous_mapping_size);
		guard.release();
		return {address, address + fsz, address + capacity, file_mapping_size, anonymous_mapping_size};
#endif
	}

	void *h_section{};
	::std::uint_least64_t capacity64{static_cast<::std::uint_least64_t>(capacity)};
	::std::uint_least64_t *maximum_size{capacity == fsz ? nullptr : __builtin_addressof(capacity64)};
	::fast_io::win32::nt::object_attributes objAttr;
	secattr_ptr pobjattr{reinterpret_cast<secattr_ptr>(options.objAttr)};
	if (pobjattr == nullptr)
	{
		objAttr = {};
		objAttr.Length = sizeof(::fast_io::win32::nt::object_attributes);
		pobjattr = __builtin_addressof(objAttr);
	}
	::std::uint_least32_t status{};
	status = ::fast_io::win32::nt::nt_create_section<(family == ::fast_io::nt_family::zw)>(__builtin_addressof(h_section), options.dwDesiredAccess, pobjattr, maximum_size, options.flProtect, options.attributes, handle);
	if (status)
	{
		throw_nt_error(status);
	}
	::fast_io::basic_nt_family_file<family, char> map_hd{h_section};
	void *p_map_address{};
	::std::size_t view_size{capacity};
	void *current_process_handle{reinterpret_cast<void *>(static_cast<::std::ptrdiff_t>(-1))};
	status = ::fast_io::win32::nt::nt_map_view_of_section<(family == ::fast_io::nt_family::zw)>(h_section, current_process_handle, __builtin_addressof(p_map_address), 0u, 0u, nullptr, __builtin_addressof(view_size), static_cast<::fast_io::win32::nt::section_inherit>(options.viewShare), 0u, options.flProtect);
	if (status)
	{
		throw_nt_error(status);
	}
	return {reinterpret_cast<char *>(p_map_address), reinterpret_cast<char *>(p_map_address) + fsz,
			reinterpret_cast<char *>(p_map_address) + capacity};
}

inline constexpr auto create_nt_default_load_address_option() noexcept
{
	::fast_io::nt_mmap_options options;
	options.dwDesiredAccess = 0x000F0000 | 0x0001 | 0x0004;
	options.flProtect = 0x08;
	options.attributes = 0x08000000;
	options.viewShare = 1;
	return options;
}

inline constexpr auto create_nt_default_load_address_option(::fast_io::file_loader_extra_bytes extra) noexcept
{
	auto options{create_nt_default_load_address_option()};
	options.extra_bytes = extra.n;
	options.padding_mode = extra.mode;
	return options;
}

template <::fast_io::nt_family family>
inline nt_file_loader_return_value_t nt_load_address_impl(void *handle)
{
	constexpr auto nt_file_loader_default_option{create_nt_default_load_address_option()};
	return nt_load_address_options_impl<family>(nt_file_loader_default_option, handle);
}

template <::fast_io::nt_family family, typename... Args>
inline auto nt_load_file_impl(Args &&...args)
{
	::fast_io::basic_nt_family_file<family, char> nf(::fast_io::freestanding::forward<Args>(args)...);
	return nt_load_address_impl<family>(nf.handle);
}

template <::fast_io::nt_family family, typename... Args>
inline auto nt_load_file_options_impl(nt_mmap_options const &options, Args &&...args)
{
	::fast_io::basic_nt_family_file<family, char> wf(::fast_io::freestanding::forward<Args>(args)...);
	return nt_load_address_options_impl<family>(options, wf.handle);
}

} // namespace win32::nt::details

struct released_nt_file_loader_mapping
{
	char *address_begin{};
	char *address_end{};
	char *address_capacity{};
	::std::size_t file_mapping_size{};
	::std::size_t anonymous_mapping_size{};
};

template <::fast_io::nt_family family>
	requires(family == ::fast_io::nt_family::nt || family == ::fast_io::nt_family::zw)
class nt_family_file_loader 
{
public:
	using value_type = char;
	using pointer = char *;
	using const_pointer = char const *;
	using const_iterator = const_pointer;
	using iterator = pointer;
	using reference = char &;
	using const_reference = char const &;
	using size_type = ::std::size_t;
	using difference_type = ::std::ptrdiff_t;
	using const_reverse_iterator = ::std::reverse_iterator<const_iterator>;
	using reverse_iterator = ::std::reverse_iterator<iterator>;
	using native_handle_type = released_nt_file_loader_mapping;

	native_handle_type storage{};

	inline constexpr nt_family_file_loader() noexcept = default;
	inline explicit constexpr nt_family_file_loader(native_handle_type mapping) noexcept
		: storage(mapping)
	{
	}
	inline explicit nt_family_file_loader(::fast_io::nt_at_entry ent)
	{
		auto ret{::fast_io::win32::nt::details::nt_load_address_impl<family>(ent.handle)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.file_mapping_size,
									  ret.anonymous_mapping_size};
	}
	inline explicit nt_family_file_loader(::fast_io::nt_fs_dirent fsdirent,
										  ::fast_io::open_mode om = ::fast_io::open_mode::in,
										  ::fast_io::perms pm = static_cast<::fast_io::perms>(436))
	{
		auto ret{::fast_io::win32::nt::details::nt_load_file_impl<family>(fsdirent, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.file_mapping_size,
									  ret.anonymous_mapping_size};
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit nt_family_file_loader(T const &filename, ::fast_io::open_mode om = ::fast_io::open_mode::in,
										  ::fast_io::perms pm = static_cast<::fast_io::perms>(436))
	{
		auto ret{::fast_io::win32::nt::details::nt_load_file_impl<family>(filename, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.file_mapping_size,
									  ret.anonymous_mapping_size};
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit nt_family_file_loader(::fast_io::nt_at_entry ent, T const &filename,
										  ::fast_io::open_mode om = ::fast_io::open_mode::in,
										  ::fast_io::perms pm = static_cast<::fast_io::perms>(436))
	{
		auto ret{::fast_io::win32::nt::details::nt_load_file_impl<family>(ent, filename, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.file_mapping_size,
									  ret.anonymous_mapping_size};
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit nt_family_file_loader(::fast_io::io_kernel_t, T const &t, 
		                                  ::fast_io::open_mode om = ::fast_io::open_mode::in,
										  ::fast_io::perms pm = static_cast<::fast_io::perms>(436))
	{
		auto ret{::fast_io::win32::nt::details::nt_load_file_impl<family>(::fast_io::io_kernel, t, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.file_mapping_size,
									  ret.anonymous_mapping_size};
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit nt_family_file_loader(::fast_io::io_kernel_t, ::fast_io::nt_at_entry ent, T const &t,
										  ::fast_io::open_mode om = ::fast_io::open_mode::in,
										  ::fast_io::perms pm = static_cast<::fast_io::perms>(436))
	{
		auto ret{::fast_io::win32::nt::details::nt_load_file_impl<family>(::fast_io::io_kernel, ent, t, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.file_mapping_size,
									  ret.anonymous_mapping_size};
	}

	inline explicit nt_family_file_loader(nt_mmap_options const &options, ::fast_io::nt_at_entry ent)
	{
		auto ret{::fast_io::win32::nt::details::nt_load_address_options_impl<family>(options, ent.handle)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.file_mapping_size,
									  ret.anonymous_mapping_size};
	}
	inline explicit nt_family_file_loader(nt_mmap_options const &options, ::fast_io::nt_fs_dirent fsdirent,
										  ::fast_io::open_mode om = ::fast_io::open_mode::in,
										  ::fast_io::perms pm = static_cast<::fast_io::perms>(436))
	{
		auto ret{::fast_io::win32::nt::details::nt_load_file_options_impl<family>(options, fsdirent, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.file_mapping_size,
									  ret.anonymous_mapping_size};
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit nt_family_file_loader(nt_mmap_options const &options, T const &filename,
										  ::fast_io::open_mode om = ::fast_io::open_mode::in,
										  ::fast_io::perms pm = static_cast<::fast_io::perms>(436))
	{
		auto ret{::fast_io::win32::nt::details::nt_load_file_options_impl<family>(options, filename, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.file_mapping_size,
									  ret.anonymous_mapping_size};
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit nt_family_file_loader(nt_mmap_options const &options, ::fast_io::nt_at_entry ent, T const &filename,
										  ::fast_io::open_mode om = ::fast_io::open_mode::in,
										  ::fast_io::perms pm = static_cast<::fast_io::perms>(436))
	{
		auto ret{::fast_io::win32::nt::details::nt_load_file_options_impl<family>(options, ent, filename, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.file_mapping_size,
									  ret.anonymous_mapping_size};
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit nt_family_file_loader(nt_mmap_options const &options, ::fast_io::io_kernel_t, T const &t,
										  ::fast_io::open_mode om = ::fast_io::open_mode::in,
										  ::fast_io::perms pm = static_cast<::fast_io::perms>(436))
	{
		auto ret{
			::fast_io::win32::nt::details::nt_load_file_options_impl<family>(options, ::fast_io::io_kernel, t, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.file_mapping_size,
									  ret.anonymous_mapping_size};
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit nt_family_file_loader(nt_mmap_options const &options, ::fast_io::io_kernel_t,
										  ::fast_io::nt_at_entry ent, T const &t, 
										  ::fast_io::open_mode om = ::fast_io::open_mode::in,
										  ::fast_io::perms pm = static_cast<::fast_io::perms>(436))
	{
		auto ret{::fast_io::win32::nt::details::nt_load_file_options_impl<family>(options, ::fast_io::io_kernel, ent, t,
																				  om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.file_mapping_size,
									  ret.anonymous_mapping_size};
	}
	inline nt_family_file_loader(nt_family_file_loader const &) = delete;
	inline nt_family_file_loader &operator=(nt_family_file_loader const &) = delete;
	inline constexpr nt_family_file_loader(nt_family_file_loader &&__restrict other) noexcept
		: storage(other.storage)
	{
		other.storage = {};
	}
	inline nt_family_file_loader &operator=(nt_family_file_loader &&__restrict other) noexcept
	{
		if (__builtin_addressof(other) == this) [[unlikely]]
		{
			return *this;
		}
		::fast_io::win32::nt::details::nt_unload_address<family>(storage.address_begin, storage.file_mapping_size,
																  storage.anonymous_mapping_size);
		storage = other.storage;
		other.storage = {};
		return *this;
	}
	inline constexpr pointer data() noexcept
	{
		return storage.address_begin;
	}
	inline constexpr const_pointer data() const noexcept
	{
		return storage.address_begin;
	}
	inline constexpr bool empty() const noexcept
	{
		return storage.address_begin == storage.address_end;
	}
	inline constexpr bool is_empty() const noexcept
	{
		return storage.address_begin == storage.address_end;
	}
	inline constexpr ::std::size_t size() const noexcept
	{
		return static_cast<::std::size_t>(storage.address_end - storage.address_begin);
	}
	inline constexpr ::std::size_t max_size() const noexcept
	{
		return SIZE_MAX;
	}
	inline constexpr ::std::size_t capacity() const noexcept
	{
		return static_cast<::std::size_t>(storage.address_capacity - storage.address_begin);
	}
	inline constexpr ::std::size_t padding_size() const noexcept
	{
		return static_cast<::std::size_t>(storage.address_capacity - storage.address_end);
	}
	inline constexpr bool has_padding(::std::size_t n) const noexcept
	{
		return n <= this->padding_size();
	}
	inline constexpr const_iterator cbegin() const noexcept
	{
		return storage.address_begin;
	}
	inline constexpr const_iterator begin() const noexcept
	{
		return storage.address_begin;
	}
	inline constexpr iterator begin() noexcept
	{
		return storage.address_begin;
	}
	inline constexpr const_iterator cend() const noexcept
	{
		return storage.address_end;
	}
	inline constexpr const_iterator end() const noexcept
	{
		return storage.address_end;
	}
	inline constexpr iterator end() noexcept
	{
		return storage.address_end;
	}
	inline constexpr const_reverse_iterator crbegin() const noexcept
	{
		return const_reverse_iterator{storage.address_end};
	}
	inline constexpr reverse_iterator rbegin() noexcept
	{
		return reverse_iterator{storage.address_end};
	}
	inline constexpr const_reverse_iterator rbegin() const noexcept
	{
		return const_reverse_iterator{storage.address_end};
	}
	inline constexpr const_reverse_iterator crend() const noexcept
	{
		return const_reverse_iterator{storage.address_begin};
	}
	inline constexpr reverse_iterator rend() noexcept
	{
		return reverse_iterator{storage.address_begin};
	}
	inline constexpr const_reverse_iterator rend() const noexcept
	{
		return const_reverse_iterator{storage.address_begin};
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]]
	inline constexpr const_reference front() const noexcept
	{
		if (storage.address_begin == storage.address_end) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return *storage.address_begin;
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]]
	inline constexpr reference front() noexcept
	{
		if (storage.address_begin == storage.address_end) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return *storage.address_begin;
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]]
	inline constexpr const_reference back() const noexcept
	{
		if (storage.address_begin == storage.address_end) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return storage.address_end[-1];
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]]
	inline constexpr reference back() noexcept
	{
		if (storage.address_begin == storage.address_end) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return storage.address_end[-1];
	}

	inline constexpr const_reference front_unchecked() const noexcept
	{
		return *storage.address_begin;
	}
	inline constexpr reference front_unchecked() noexcept
	{
		return *storage.address_begin;
	}
	inline constexpr const_reference back_unchecked() const noexcept
	{
		return storage.address_end[-1];
	}
	inline constexpr reference back_unchecked() noexcept
	{
		return storage.address_end[-1];
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]]
	inline constexpr reference operator[](size_type size) noexcept
	{
		if (static_cast<size_type>(storage.address_end - storage.address_begin) <= size) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return storage.address_begin[size];
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]]
	inline constexpr const_reference operator[](size_type size) const noexcept
	{
		if (static_cast<size_type>(storage.address_end - storage.address_begin) <= size) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return storage.address_begin[size];
	}

	inline constexpr reference index_unchecked(size_type size) noexcept
	{
		return storage.address_begin[size];
	}
	inline constexpr const_reference index_unchecked(size_type size) const noexcept
	{
		return storage.address_begin[size];
	}

#if __has_cpp_attribute(nodiscard)
	[[nodiscard]]
#endif
	inline constexpr native_handle_type release() noexcept
	{
		native_handle_type temp{this->storage};
		this->storage = {};
		return temp;
	}

	inline void close()
	{
		::fast_io::win32::nt::details::nt_unload_address<family>(storage.address_begin, storage.file_mapping_size,
																  storage.anonymous_mapping_size);
		storage = {};
	}
	inline ~nt_family_file_loader()
	{
		::fast_io::win32::nt::details::nt_unload_address<family>(storage.address_begin, storage.file_mapping_size,
																  storage.anonymous_mapping_size);
	}
};

template <::fast_io::nt_family family>
inline constexpr basic_io_scatter_t<char> print_alias_define(::fast_io::io_alias_t,
															 nt_family_file_loader<family> const &load) noexcept
{
	return {load.data(), load.size()};
}

using nt_file_loader = nt_family_file_loader<::fast_io::nt_family::nt>;
using zw_file_loader = nt_family_file_loader<::fast_io::nt_family::zw>;

namespace freestanding
{
template <::fast_io::nt_family family>
struct is_trivially_copyable_or_relocatable<nt_family_file_loader<family>>
{
	inline static constexpr bool value = true;
};

template <::fast_io::nt_family family>
struct is_zero_default_constructible<nt_family_file_loader<family>>
{
	inline static constexpr bool value = true;
};
} // namespace freestanding

} // namespace fast_io
