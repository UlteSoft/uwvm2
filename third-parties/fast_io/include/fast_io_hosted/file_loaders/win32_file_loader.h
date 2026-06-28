#pragma once

namespace fast_io
{

namespace win32::details
{

struct win32_file_loader_return_value_t
{
	char *address_begin{};
	char *address_end{};
	char *address_capacity{};
	::std::size_t file_mapping_size{};
	::std::size_t anonymous_mapping_size{};
};

inline ::std::size_t win32_file_loader_add_extra_bytes(::std::size_t file_size, ::std::size_t extra_bytes)
{
	if (SIZE_MAX - file_size < extra_bytes)
	{
		throw_win32_error(0x000000DF);
	}
	return file_size + extra_bytes;
}

inline ::std::size_t win32_file_loader_round_up_to_page(::std::size_t value, ::std::size_t page_size)
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
		throw_win32_error(0x000000DF);
	}
	return value + inc;
}

inline bool win32_file_loader_is_private_write(win32_mmap_options const &options) noexcept
{
	return options.flProtect == 0x08u /*PAGE_WRITECOPY*/ ||
		   options.flProtect == 0x80u /*PAGE_EXECUTE_WRITECOPY*/;
}

inline ::std::uint_least32_t win32_file_loader_writecopy_protect(::std::uint_least32_t flprotect) noexcept
{
	if (flprotect & (0x10u | 0x20u | 0x40u | 0x80u))
	{
		return 0x80u /*PAGE_EXECUTE_WRITECOPY*/;
	}
	return 0x08u /*PAGE_WRITECOPY*/;
}

inline ::std::uint_least32_t win32_file_loader_file_protect(win32_mmap_options const &options) noexcept
{
	if (win32_file_loader_is_private_write(options))
	{
		return win32_file_loader_writecopy_protect(options.flProtect);
	}
	return options.flProtect;
}

inline ::std::uint_least32_t win32_file_loader_view_access(win32_mmap_options const &options) noexcept
{
	if (win32_file_loader_is_private_write(options))
	{
		::std::uint_least32_t access{0x00000001u /*FILE_MAP_COPY*/};
		if (options.dwDesiredAccess & 0x00000020u /*FILE_MAP_EXECUTE*/)
		{
			access |= 0x00000020u /*FILE_MAP_EXECUTE*/;
		}
		return access;
	}
	return options.dwDesiredAccess;
}

inline ::std::uint_least32_t win32_file_loader_anonymous_protect(::std::uint_least32_t flprotect) noexcept
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

inline ::std::size_t win32_file_loader_get_page_size() noexcept
{
	::fast_io::win32::nt::system_basic_information sb{};
	auto status{::fast_io::win32::nt::nt_query_system_information<false>(
		::fast_io::win32::nt::system_information_class::SystemBasicInformation, __builtin_addressof(sb),
		static_cast<::std::uint_least32_t>(sizeof(sb)), nullptr)};
	if (status == 0 && sb.PageSize != 0)
	{
		return sb.PageSize;
	}
	return 4096u;
}

inline constexpr void *win32_file_loader_current_process() noexcept
{
	return reinterpret_cast<void *>(static_cast<::std::ptrdiff_t>(-1));
}

using win32_file_loader_virtual_alloc2_type = void *(FAST_IO_WINSTDCALL *)(void *, void *, ::std::size_t,
																		   ::std::uint_least32_t,
																		   ::std::uint_least32_t, void *,
																		   ::std::uint_least32_t) noexcept;
using win32_file_loader_map_view_of_file3_type = void *(FAST_IO_WINSTDCALL *)(void *, void *, void *,
																			  ::std::uint_least64_t, ::std::size_t,
																			  ::std::uint_least32_t,
																			  ::std::uint_least32_t, void *,
																			  ::std::uint_least32_t) noexcept;

inline ::fast_io::win32::farproc win32_file_loader_get_kernelbase_proc(char const *name)
{
	static void *kernelbase_module{[]() noexcept {
		void *module{::fast_io::win32::GetModuleHandleW(u"KernelBase.dll")};
		if (module == nullptr)
		{
			module = ::fast_io::win32::LoadLibraryW(u"KernelBase.dll");
		}
		return module;
	}()};
	if (kernelbase_module == nullptr)
	{
		throw_win32_error(0x00000032u /*ERROR_NOT_SUPPORTED*/);
	}
	auto proc{::fast_io::win32::GetProcAddress(kernelbase_module, name)};
	if (proc == nullptr)
	{
		throw_win32_error(0x00000032u /*ERROR_NOT_SUPPORTED*/);
	}
	return proc;
}

inline win32_file_loader_virtual_alloc2_type win32_file_loader_virtual_alloc2()
{
	return reinterpret_cast<win32_file_loader_virtual_alloc2_type>(
		win32_file_loader_get_kernelbase_proc("VirtualAlloc2"));
}

inline win32_file_loader_map_view_of_file3_type win32_file_loader_map_view_of_file3()
{
	return reinterpret_cast<win32_file_loader_map_view_of_file3_type>(
		win32_file_loader_get_kernelbase_proc("MapViewOfFile3"));
}

inline void win32_unload_address(void const *address, ::std::size_t file_mapping_size = 0,
								 ::std::size_t anonymous_mapping_size = 0) noexcept
{
	if (address)
	{
		auto base{const_cast<char *>(reinterpret_cast<char const *>(address))};
		if (file_mapping_size || anonymous_mapping_size)
		{
			if (file_mapping_size)
			{
				::fast_io::win32::UnmapViewOfFile(base);
			}
			if (anonymous_mapping_size)
			{
				::fast_io::win32::VirtualFree(base + file_mapping_size, 0, 0x00008000u /*MEM_RELEASE*/);
			}
		}
		else
		{
			::fast_io::win32::UnmapViewOfFile(address);
		}
	}
}

struct win32_file_loader_composite_guard
{
	char *address{};
	::std::size_t prefix_placeholder_size{};
	::std::size_t tail_placeholder_offset{};
	::std::size_t tail_placeholder_size{};
	::std::size_t file_mapping_size{};
	::std::size_t anonymous_mapping_size{};
	inline explicit constexpr win32_file_loader_composite_guard() noexcept = default;
	inline explicit constexpr win32_file_loader_composite_guard(char *addr, ::std::size_t n) noexcept
		: address{addr}, prefix_placeholder_size{n}
	{}
	inline win32_file_loader_composite_guard(win32_file_loader_composite_guard const &) = delete;
	inline win32_file_loader_composite_guard &operator=(win32_file_loader_composite_guard const &) = delete;
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
	inline ~win32_file_loader_composite_guard()
	{
		if (address == nullptr)
		{
			return;
		}
		if (file_mapping_size)
		{
			::fast_io::win32::UnmapViewOfFile(address);
		}
		if (prefix_placeholder_size)
		{
			::fast_io::win32::VirtualFree(address, 0, 0x00008000u /*MEM_RELEASE*/);
		}
		auto tail{address + tail_placeholder_offset};
		if (anonymous_mapping_size)
		{
			::fast_io::win32::VirtualFree(tail, 0, 0x00008000u /*MEM_RELEASE*/);
		}
		if (tail_placeholder_size)
		{
			::fast_io::win32::VirtualFree(tail, 0, 0x00008000u /*MEM_RELEASE*/);
		}
	}
};

inline win32_file_loader_return_value_t win32_load_address_common_options_impl(void *hfilemappingobj,
																			   ::std::size_t file_size,
																			   ::std::size_t capacity,
																			   ::std::uint_least32_t dwDesiredAccess)
{
	if (hfilemappingobj == nullptr)
	{
		throw_win32_error();
	}
	::fast_io::win32_file map_hd{hfilemappingobj};
	auto base_ptr{::fast_io::win32::MapViewOfFile(hfilemappingobj, dwDesiredAccess, 0, 0, capacity)};
	if (base_ptr == nullptr)
	{
		throw_win32_error();
	}
	return {reinterpret_cast<char *>(base_ptr), reinterpret_cast<char *>(base_ptr) + file_size,
			reinterpret_cast<char *>(base_ptr) + capacity};
}

inline win32_file_loader_return_value_t win32_load_address_common_impl(void *hfilemappingobj, ::std::size_t file_size)
{
	return win32_load_address_common_options_impl(hfilemappingobj, file_size, file_size, 1);
}

template <win32_family family>
inline win32_file_loader_return_value_t win32_load_address_impl(void *handle)
{
	::std::size_t file_size{win32_load_file_get_file_size(handle)};
	if (file_size == 0)
	{
		return {nullptr, nullptr, nullptr};
	}
	if constexpr (family == win32_family::wide_nt)
	{
		return win32_load_address_common_impl(
			::fast_io::win32::CreateFileMappingW(handle, nullptr, 0x08, 0, 0, nullptr), file_size);
	}
	else
	{
		return win32_load_address_common_impl(
			::fast_io::win32::CreateFileMappingA(handle, nullptr, 0x08, 0, 0, nullptr), file_size);
	}
}

template <win32_family family, typename... Args>
inline auto win32_load_file_impl(Args &&...args)
{
	::fast_io::basic_win32_family_file<family, char> wf(::fast_io::freestanding::forward<Args>(args)...);
	return win32_load_address_impl<family>(wf.handle);
}

template <win32_family family>
inline win32_file_loader_return_value_t win32_load_address_options_impl(win32_mmap_options const &options, void *handle)
{
	::std::size_t file_size{win32_load_file_get_file_size(handle)};
	::std::size_t const capacity{win32_file_loader_add_extra_bytes(file_size, options.extra_bytes)};
	if (capacity == 0)
	{
		return {nullptr, nullptr, nullptr};
	}
	if (options.extra_bytes)
	{
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0A00
		throw_win32_error(0x00000032u /*ERROR_NOT_SUPPORTED*/);
#else
		if constexpr (family == win32_family::ansi_9x)
		{
			throw_win32_error(0x00000032u /*ERROR_NOT_SUPPORTED*/);
		}
		else
		{
			::std::size_t const page_size{win32_file_loader_get_page_size()};
			::std::size_t const file_mapping_size{win32_file_loader_round_up_to_page(file_size, page_size)};
			::std::size_t const total_mapping_size{win32_file_loader_round_up_to_page(capacity, page_size)};
			::std::size_t const anonymous_mapping_size{total_mapping_size - file_mapping_size};
			::std::uint_least32_t const file_protect{win32_file_loader_file_protect(options)};
			::std::uint_least32_t const view_access{win32_file_loader_view_access(options)};
			::std::uint_least32_t const anonymous_protect{win32_file_loader_anonymous_protect(file_protect)};
			using secattr_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= ::fast_io::win32::security_attributes *;
			if (capacity <= file_mapping_size)
			{
				void *hfilemappingobj{};
				if constexpr (family == win32_family::wide_nt)
				{
					using char16_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
						[[__gnu__::__may_alias__]]
#endif
						= char16_t const *;
					hfilemappingobj = ::fast_io::win32::CreateFileMappingW(
						handle, reinterpret_cast<secattr_ptr>(options.lpFileMappingAttributes),
						file_protect, 0, 0, reinterpret_cast<char16_const_ptr>(options.lpName));
				}
				else
				{
					hfilemappingobj = ::fast_io::win32::CreateFileMappingA(
						handle, reinterpret_cast<secattr_ptr>(options.lpFileMappingAttributes),
						file_protect, 0, 0, reinterpret_cast<char const *>(options.lpName));
				}
				if (hfilemappingobj == nullptr)
				{
					throw_win32_error();
				}
				::fast_io::win32_file map_hd{hfilemappingobj};
				auto base_ptr{::fast_io::win32::MapViewOfFile(hfilemappingobj, view_access, 0, 0, 0)};
				if (base_ptr == nullptr)
				{
					throw_win32_error();
				}
				auto address{reinterpret_cast<char *>(base_ptr)};
				return {address, address + file_size, address + capacity, file_mapping_size, 0};
			}
			auto virtual_alloc2{win32_file_loader_virtual_alloc2()};
			auto map_view_of_file3{win32_file_loader_map_view_of_file3()};
			auto address{reinterpret_cast<char *>(virtual_alloc2(
				win32_file_loader_current_process(), nullptr, total_mapping_size,
				0x00002000u | 0x00040000u /*MEM_RESERVE | MEM_RESERVE_PLACEHOLDER*/,
				0x01u /*PAGE_NOACCESS*/, nullptr, 0))};
			if (address == nullptr)
			{
				throw_win32_error();
			}
			win32_file_loader_composite_guard guard{address, total_mapping_size};
			if (file_mapping_size)
			{
				if (!::fast_io::win32::VirtualFree(address + file_mapping_size, anonymous_mapping_size,
												   0x00008000u | 0x00000002u /*MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER*/))
				{
					throw_win32_error();
				}
				guard.split_tail_placeholder(file_mapping_size, anonymous_mapping_size);
				if (!::fast_io::win32::VirtualFree(address, 0, 0x00008000u /*MEM_RELEASE*/))
				{
					throw_win32_error();
				}
				guard.mark_prefix_placeholder_released();
				void *hfilemappingobj{};
				if constexpr (family == win32_family::wide_nt)
				{
					using char16_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
						[[__gnu__::__may_alias__]]
#endif
						= char16_t const *;
					hfilemappingobj = ::fast_io::win32::CreateFileMappingW(
						handle, reinterpret_cast<secattr_ptr>(options.lpFileMappingAttributes),
						file_protect, 0, 0, reinterpret_cast<char16_const_ptr>(options.lpName));
				}
				else
				{
					hfilemappingobj = ::fast_io::win32::CreateFileMappingA(
						handle, reinterpret_cast<secattr_ptr>(options.lpFileMappingAttributes),
						file_protect, 0, 0, reinterpret_cast<char const *>(options.lpName));
				}
				if (hfilemappingobj == nullptr)
				{
					throw_win32_error();
				}
				::fast_io::win32_file map_hd{hfilemappingobj};
				auto prefix_address{reinterpret_cast<char *>(map_view_of_file3(
					hfilemappingobj, win32_file_loader_current_process(), address, 0, 0, 0,
					file_protect, nullptr, 0))};
				if (prefix_address != address)
				{
					throw_win32_error();
				}
				guard.mark_file_mapped(file_mapping_size);
			}
			auto tail_address{reinterpret_cast<char *>(virtual_alloc2(
				win32_file_loader_current_process(), address + file_mapping_size, anonymous_mapping_size,
				0x00001000u | 0x00002000u | 0x00004000u /*MEM_COMMIT | MEM_RESERVE | MEM_REPLACE_PLACEHOLDER*/,
				anonymous_protect, nullptr, 0))};
			if (tail_address != address + file_mapping_size)
			{
				throw_win32_error();
			}
			guard.mark_anonymous_mapped(anonymous_mapping_size);
			guard.release();
			return {address, address + file_size, address + capacity, file_mapping_size, anonymous_mapping_size};
		}
#endif
	}
	::std::uint_least32_t capacity_high{};
	::std::uint_least32_t capacity_low{};
	if (capacity != file_size)
	{
		auto const capacity64{static_cast<::std::uint_least64_t>(capacity)};
		capacity_high = static_cast<::std::uint_least32_t>(capacity64 >> 32u);
		capacity_low = static_cast<::std::uint_least32_t>(capacity64);
	}
	using secattr_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= ::fast_io::win32::security_attributes *;
	if constexpr (family == win32_family::wide_nt)
	{
		using char16_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char16_t const *;
		return win32_load_address_common_options_impl(
			::fast_io::win32::CreateFileMappingW(handle, reinterpret_cast<secattr_ptr>(options.lpFileMappingAttributes),
												 options.flProtect, capacity_high, capacity_low,
												 reinterpret_cast<char16_const_ptr>(options.lpName)),
			file_size, capacity, options.dwDesiredAccess);
	}
	else
	{
		return win32_load_address_common_options_impl(
			::fast_io::win32::CreateFileMappingA(handle, reinterpret_cast<secattr_ptr>(options.lpFileMappingAttributes),
												 options.flProtect, capacity_high, capacity_low,
												 reinterpret_cast<char const *>(options.lpName)),
			file_size, capacity, options.dwDesiredAccess);
	}
}

template <win32_family family, typename... Args>
inline auto win32_load_file_options_impl(win32_mmap_options const &options, Args &&...args)
{
	::fast_io::basic_win32_family_file<family, char> wf(::fast_io::freestanding::forward<Args>(args)...);
	return win32_load_address_options_impl<family>(options, wf.handle);
}

} // namespace win32::details

template <win32_family family>
class win32_family_file_loader 
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

	pointer address_begin{};
	pointer address_end{};
	pointer address_capacity{};
	::std::size_t file_mapping_size{};
	::std::size_t anonymous_mapping_size{};
	inline constexpr win32_family_file_loader() noexcept = default;
	inline explicit win32_family_file_loader(nt_at_entry ent)
	{
		auto ret{::fast_io::win32::details::win32_load_address_impl<family>(ent.handle)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
		file_mapping_size = ret.file_mapping_size;
		anonymous_mapping_size = ret.anonymous_mapping_size;
	}
	inline explicit win32_family_file_loader(nt_fs_dirent fsdirent, open_mode om = open_mode::in,
											 perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::win32::details::win32_load_file_impl<family>(fsdirent, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
		file_mapping_size = ret.file_mapping_size;
		anonymous_mapping_size = ret.anonymous_mapping_size;
	}
	inline explicit win32_family_file_loader(win32_9xa_fs_dirent fsdirent, open_mode om = open_mode::in,
											 perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::win32::details::win32_load_file_impl<family>(fsdirent, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
		file_mapping_size = ret.file_mapping_size;
		anonymous_mapping_size = ret.anonymous_mapping_size;
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit win32_family_file_loader(T const &filename, open_mode om = open_mode::in,
											 perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::win32::details::win32_load_file_impl<family>(filename, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
		file_mapping_size = ret.file_mapping_size;
		anonymous_mapping_size = ret.anonymous_mapping_size;
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit win32_family_file_loader(nt_at_entry ent, T const &filename, open_mode om = open_mode::in,
											 perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::win32::details::win32_load_file_impl<family>(ent, filename, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
		file_mapping_size = ret.file_mapping_size;
		anonymous_mapping_size = ret.anonymous_mapping_size;
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit win32_family_file_loader(win32_9xa_at_entry ent, T const &filename, open_mode om = open_mode::in,
											 perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::win32::details::win32_load_file_impl<family>(ent, filename, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
		file_mapping_size = ret.file_mapping_size;
		anonymous_mapping_size = ret.anonymous_mapping_size;
	}
	inline explicit win32_family_file_loader(win32_mmap_options const &options, nt_at_entry ent)
	{
		auto ret{::fast_io::win32::details::win32_load_address_options_impl<family>(options, ent.handle)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
		file_mapping_size = ret.file_mapping_size;
		anonymous_mapping_size = ret.anonymous_mapping_size;
	}
	inline explicit win32_family_file_loader(win32_mmap_options const &options, nt_fs_dirent fsdirent,
											 open_mode om = open_mode::in, perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::win32::details::win32_load_file_options_impl<family>(options, fsdirent, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
		file_mapping_size = ret.file_mapping_size;
		anonymous_mapping_size = ret.anonymous_mapping_size;
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit win32_family_file_loader(win32_mmap_options const &options, T const &filename,
											 open_mode om = open_mode::in, perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::win32::details::win32_load_file_options_impl<family>(options, filename, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
		file_mapping_size = ret.file_mapping_size;
		anonymous_mapping_size = ret.anonymous_mapping_size;
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit win32_family_file_loader(win32_mmap_options const &options, nt_at_entry ent, T const &filename,
											 open_mode om = open_mode::in, perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::win32::details::win32_load_file_options_impl<family>(options, ent, filename, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
		file_mapping_size = ret.file_mapping_size;
		anonymous_mapping_size = ret.anonymous_mapping_size;
	}

	inline win32_family_file_loader(win32_family_file_loader const &) = delete;
	inline win32_family_file_loader &operator=(win32_family_file_loader const &) = delete;
	inline constexpr win32_family_file_loader(win32_family_file_loader &&__restrict other) noexcept
		: address_begin(other.address_begin), address_end(other.address_end),
		  address_capacity(other.address_capacity), file_mapping_size(other.file_mapping_size),
		  anonymous_mapping_size(other.anonymous_mapping_size)
	{
		other.address_capacity = other.address_end = other.address_begin = nullptr;
		other.file_mapping_size = {};
		other.anonymous_mapping_size = {};
	}
	inline win32_family_file_loader &operator=(win32_family_file_loader &&__restrict other) noexcept
	{
		if (__builtin_addressof(other) == this) [[unlikely]]
		{
			return *this;
		}
		::fast_io::win32::details::win32_unload_address(address_begin, file_mapping_size,
														 anonymous_mapping_size);
		address_begin = other.address_begin;
		address_end = other.address_end;
		address_capacity = other.address_capacity;
		file_mapping_size = other.file_mapping_size;
		anonymous_mapping_size = other.anonymous_mapping_size;
		other.address_capacity = other.address_end = other.address_begin = nullptr;
		other.file_mapping_size = {};
		other.anonymous_mapping_size = {};
		return *this;
	}
	inline constexpr pointer data() noexcept
	{
		return address_begin;
	}
	inline constexpr const_pointer data() const noexcept
	{
		return address_begin;
	}
	inline constexpr bool empty() const noexcept
	{
		return address_begin == address_end;
	}
	inline constexpr bool is_empty() const noexcept
	{
		return address_begin == address_end;
	}
	inline constexpr ::std::size_t size() const noexcept
	{
		return static_cast<::std::size_t>(address_end - address_begin);
	}
	inline constexpr ::std::size_t max_size() const noexcept
	{
		return SIZE_MAX;
	}
	inline constexpr ::std::size_t capacity() const noexcept
	{
		return static_cast<::std::size_t>(address_capacity - address_begin);
	}
	inline constexpr ::std::size_t padding_size() const noexcept
	{
		return static_cast<::std::size_t>(address_capacity - address_end);
	}
	inline constexpr bool has_padding(::std::size_t n) const noexcept
	{
		return n <= this->padding_size();
	}
	inline constexpr const_iterator cbegin() const noexcept
	{
		return address_begin;
	}
	inline constexpr const_iterator begin() const noexcept
	{
		return address_begin;
	}
	inline constexpr iterator begin() noexcept
	{
		return address_begin;
	}
	inline constexpr const_iterator cend() const noexcept
	{
		return address_end;
	}
	inline constexpr const_iterator end() const noexcept
	{
		return address_end;
	}
	inline constexpr iterator end() noexcept
	{
		return address_end;
	}
	inline constexpr const_reverse_iterator crbegin() const noexcept
	{
		return const_reverse_iterator{address_end};
	}
	inline constexpr reverse_iterator rbegin() noexcept
	{
		return reverse_iterator{address_end};
	}
	inline constexpr const_reverse_iterator rbegin() const noexcept
	{
		return const_reverse_iterator{address_end};
	}
	inline constexpr const_reverse_iterator crend() const noexcept
	{
		return const_reverse_iterator{address_begin};
	}
	inline constexpr reverse_iterator rend() noexcept
	{
		return reverse_iterator{address_begin};
	}
	inline constexpr const_reverse_iterator rend() const noexcept
	{
		return const_reverse_iterator{address_begin};
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]]
	inline constexpr const_reference front() const noexcept
	{
		if (address_begin == address_end) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return *address_begin;
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]]
	inline constexpr reference front() noexcept
	{
		if (address_begin == address_end) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return *address_begin;
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]]
	inline constexpr const_reference back() const noexcept
	{
		if (address_begin == address_end) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return address_end[-1];
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]]
	inline constexpr reference back() noexcept
	{
		if (address_begin == address_end) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return address_end[-1];
	}

	inline constexpr const_reference front_unchecked() const noexcept
	{
		return *address_begin;
	}
	inline constexpr reference front_unchecked() noexcept
	{
		return *address_begin;
	}
	inline constexpr const_reference back_unchecked() const noexcept
	{
		return address_end[-1];
	}
	inline constexpr reference back_unchecked() noexcept
	{
		return address_end[-1];
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]]
	inline constexpr reference operator[](size_type size) noexcept
	{
		if (static_cast<size_type>(address_end - address_begin) <= size) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return address_begin[size];
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]]
	inline constexpr const_reference operator[](size_type size) const noexcept
	{
		if (static_cast<size_type>(address_end - address_begin) <= size) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return address_begin[size];
	}

	inline constexpr reference index_unchecked(size_type size) noexcept
	{
		return address_begin[size];
	}
	inline constexpr const_reference index_unchecked(size_type size) const noexcept
	{
		return address_begin[size];
	}
#if __has_cpp_attribute(nodiscard)
	[[nodiscard]]
#endif
	inline constexpr pointer release() noexcept
	{
		auto temp{this->address_begin};
		this->address_capacity = this->address_end = this->address_begin = nullptr;
		this->file_mapping_size = {};
		this->anonymous_mapping_size = {};
		return temp;
	}

	inline void close()
	{
		::fast_io::win32::details::win32_unload_address(address_begin, file_mapping_size,
														 anonymous_mapping_size);
		address_capacity = address_end = address_begin = nullptr;
		file_mapping_size = {};
		anonymous_mapping_size = {};
	}
	inline ~win32_family_file_loader()
	{
		win32::details::win32_unload_address(address_begin, file_mapping_size, anonymous_mapping_size);
	}
};

template <win32_family family>
inline constexpr basic_io_scatter_t<char> print_alias_define(io_alias_t,
															 win32_family_file_loader<family> const &load) noexcept
{
	return {load.data(), load.size()};
}

using win32_file_loader_9xa = win32_family_file_loader<win32_family::ansi_9x>;
using win32_file_loader_ntw = win32_family_file_loader<win32_family::wide_nt>;
using win32_file_loader = win32_family_file_loader<win32_family::native>;

namespace freestanding
{
template <::fast_io::win32_family family>
struct is_trivially_copyable_or_relocatable<win32_family_file_loader<family>>
{
	inline static constexpr bool value = true;
};

template <::fast_io::win32_family family>
struct is_zero_default_constructible<win32_family_file_loader<family>>
{
	inline static constexpr bool value = true;
};
} // namespace freestanding

} // namespace fast_io
