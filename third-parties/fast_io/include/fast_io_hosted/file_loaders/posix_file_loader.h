#pragma once

namespace fast_io
{

namespace details
{

struct posix_file_loader_return_value_t
{
	char *address_begin;
	char *address_end;
	char *address_capacity;
};

inline constexpr char *posix_file_loader_empty_address() noexcept
{
	return ::std::bit_cast<char *>(static_cast<::std::ptrdiff_t>(-1));
}

inline ::std::size_t posix_file_loader_get_page_size([[maybe_unused]] int flags) noexcept
{
#ifdef MAP_HUGETLB
	if ((flags & MAP_HUGETLB) == MAP_HUGETLB)
	{
#ifdef MAP_HUGE_1GB
		if ((flags & MAP_HUGE_1GB) == MAP_HUGE_1GB)
		{
			return static_cast<::std::size_t>(1u) << 30u;
		}
#endif
#ifdef MAP_HUGE_2MB
		if ((flags & MAP_HUGE_2MB) == MAP_HUGE_2MB)
		{
			return static_cast<::std::size_t>(2u) << 20u;
		}
#endif
	}
#endif
#if defined(_SC_PAGESIZE)
	long const ret{::fast_io::noexcept_call(::sysconf, _SC_PAGESIZE)};
	if (ret > 0)
	{
		return static_cast<::std::size_t>(ret);
	}
#elif defined(_SC_PAGE_SIZE)
	long const ret{::fast_io::noexcept_call(::sysconf, _SC_PAGE_SIZE)};
	if (ret > 0)
	{
		return static_cast<::std::size_t>(ret);
	}
#endif
#if defined(PAGE_SIZE)
	return PAGE_SIZE;
#else
	return 4096u;
#endif
}

inline ::std::size_t posix_file_loader_add_extra_bytes(::std::size_t file_size, ::std::size_t extra_bytes)
{
	if (SIZE_MAX - file_size < extra_bytes)
	{
		throw_posix_error(EINVAL);
	}
	return file_size + extra_bytes;
}

inline ::std::size_t posix_file_loader_round_up_to_page(::std::size_t value, ::std::size_t page_size)
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
		throw_posix_error(EINVAL);
	}
	return value + inc;
}

inline int posix_file_loader_make_anonymous_flags(int flags, ::fast_io::file_loader_padding_mode padding_mode) noexcept
{
#ifdef MAP_PRIVATE
	flags &= ~MAP_PRIVATE;
#endif
#ifdef MAP_SHARED
	flags &= ~MAP_SHARED;
#endif
#ifdef MAP_SHARED_VALIDATE
	flags &= ~MAP_SHARED_VALIDATE;
#endif
#ifdef MAP_PRIVATE
	flags |= MAP_PRIVATE;
#endif
#ifdef MAP_ANONYMOUS
	flags |= MAP_ANONYMOUS;
#elif defined(MAP_ANON)
	flags |= MAP_ANON;
#endif
#ifdef MAP_UNINITIALIZED
	if (padding_mode == ::fast_io::file_loader_padding_mode::zero)
	{
		flags &= ~MAP_UNINITIALIZED;
	}
#else
	static_cast<void>(padding_mode);
#endif
	return flags;
}

inline int posix_file_loader_make_fixed_flags(int flags) noexcept
{
#ifdef MAP_FIXED_NOREPLACE
	flags &= ~MAP_FIXED_NOREPLACE;
#endif
#ifdef MAP_FIXED
	flags |= MAP_FIXED;
#endif
	return flags;
}

struct posix_file_loader_mmap_guard
{
	char *address{};
	::std::size_t bytes{};
	inline explicit constexpr posix_file_loader_mmap_guard() noexcept = default;
	inline constexpr posix_file_loader_mmap_guard(char *addr, ::std::size_t n) noexcept
		: address(addr), bytes(n)
	{}
	inline posix_file_loader_mmap_guard(posix_file_loader_mmap_guard const &) = delete;
	inline posix_file_loader_mmap_guard &operator=(posix_file_loader_mmap_guard const &) = delete;
	inline constexpr void release() noexcept
	{
		address = nullptr;
		bytes = 0;
	}
	inline ~posix_file_loader_mmap_guard()
	{
		if (address != nullptr)
		{
			sys_munmap_nothrow(address, bytes);
		}
	}
};

inline posix_file_loader_return_value_t posix_load_address_options(int fd, ::std::size_t file_size,
																   ::fast_io::posix_mmap_options const &options)
{
	::std::size_t const capacity{posix_file_loader_add_extra_bytes(file_size, options.extra_bytes)};
	if (capacity == 0)
	{
		auto const empty_address{posix_file_loader_empty_address()};
		return {empty_address, empty_address, empty_address};
	}
	if (options.extra_bytes == 0)
	{
		auto add{::std::bit_cast<char *>(sys_mmap(options.addr, file_size, options.prot, options.flags, fd, 0))};
		return {add, add + file_size, add + file_size};
	}
#ifndef MAP_FIXED
	throw_posix_error(EINVAL);
#else
	::std::size_t const page_size{posix_file_loader_get_page_size(options.flags)};
	::std::size_t const file_mapping_size{posix_file_loader_round_up_to_page(file_size, page_size)};
	::std::size_t const total_mapping_size{posix_file_loader_round_up_to_page(capacity, page_size)};
	int const reserve_flags{posix_file_loader_make_anonymous_flags(options.flags, options.padding_mode)};
	auto const reserved{::std::bit_cast<char *>(sys_mmap(options.addr, total_mapping_size, PROT_NONE,
														 reserve_flags, -1, 0))};
	posix_file_loader_mmap_guard guard{reserved, total_mapping_size};
	if (file_mapping_size != 0)
	{
		sys_mmap(reserved, file_mapping_size, options.prot,
				 posix_file_loader_make_fixed_flags(options.flags), fd, 0);
	}
	if (file_mapping_size != total_mapping_size)
	{
		sys_mmap(reserved + file_mapping_size, total_mapping_size - file_mapping_size, options.prot,
				 posix_file_loader_make_fixed_flags(
					 posix_file_loader_make_anonymous_flags(options.flags, options.padding_mode)),
				 -1, 0);
	}
	guard.release();
	return {reserved, reserved + file_size, reserved + capacity};
#endif
}

inline posix_file_loader_return_value_t posix_load_address(int fd, ::std::size_t file_size)
{
	if (file_size == 0)
	{
		auto const empty_address{posix_file_loader_empty_address()};
		return {empty_address, empty_address, empty_address};
	}
	auto add{::std::bit_cast<char *>(sys_mmap(nullptr, file_size, PROT_READ | PROT_WRITE,
											  MAP_PRIVATE
#if defined(MAP_POPULATE)
												| MAP_POPULATE
#endif
											,
											  fd, 0))};
	return {add, add + file_size, add + file_size};
}

inline void posix_unload_address(void *address, [[maybe_unused]] ::std::size_t file_size) noexcept
{
	if (address != (void *)-1) [[likely]]
	{
		sys_munmap_nothrow(address, file_size);
	}
}

inline posix_file_loader_return_value_t posix_load_address_impl(int fd)
{
	::std::size_t size{posix_loader_get_file_size(fd)};
	return posix_load_address(fd, size);
}

template <typename... Args>
inline auto posix_load_file_impl(Args &&...args)
{
	posix_file pf(::fast_io::freestanding::forward<Args>(args)...);
	return posix_load_address_impl(pf.fd);
}

inline posix_file_loader_return_value_t posix_load_address_options_impl(::fast_io::posix_mmap_options const &options,
																		int fd)
{
	::std::size_t size{posix_loader_get_file_size(fd)};
	return posix_load_address_options(fd, size, options);
}

template <typename... Args>
inline auto posix_load_file_options_impl(::fast_io::posix_mmap_options const &options, Args &&...args)
{
	posix_file pf(::fast_io::freestanding::forward<Args>(args)...);
	return posix_load_address_options_impl(options, pf.fd);
}

} // namespace details

class posix_file_loader 
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
	inline explicit constexpr posix_file_loader() noexcept
		: address_begin(::std::bit_cast<char *>(static_cast<::std::ptrdiff_t>(-1))),
		  address_end(::std::bit_cast<char *>(static_cast<::std::ptrdiff_t>(-1))),
		  address_capacity(::std::bit_cast<char *>(static_cast<::std::ptrdiff_t>(-1)))
	{
	}
	inline explicit posix_file_loader(posix_at_entry pate)
	{
		auto ret{::fast_io::details::posix_load_address_impl(pate.fd)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
	}
	inline explicit posix_file_loader(native_fs_dirent fsdirent, open_mode om = open_mode::in,
									  perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::details::posix_load_file_impl(fsdirent, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit posix_file_loader(T const &filename, open_mode om = open_mode::in,
									  perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::details::posix_load_file_impl(filename, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit posix_file_loader(native_at_entry ent, T const &filename, open_mode om = open_mode::in,
									  perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::details::posix_load_file_impl(ent, filename, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
	}
	inline explicit posix_file_loader(posix_mmap_options const &options, posix_at_entry pate)
	{
		auto ret{::fast_io::details::posix_load_address_options_impl(options, pate.fd)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
	}
	inline explicit posix_file_loader(posix_mmap_options const &options, native_fs_dirent fsdirent,
									  open_mode om = open_mode::in, perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::details::posix_load_file_options_impl(options, fsdirent, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit posix_file_loader(posix_mmap_options const &options, T const &filename,
									  open_mode om = open_mode::in, perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::details::posix_load_file_options_impl(options, filename, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit posix_file_loader(posix_mmap_options const &options, native_at_entry ent, T const &filename,
									  open_mode om = open_mode::in, perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::details::posix_load_file_options_impl(options, ent, filename, om, pm)};
		address_begin = ret.address_begin;
		address_end = ret.address_end;
		address_capacity = ret.address_capacity;
	}

	inline posix_file_loader(posix_file_loader const &) = delete;
	inline posix_file_loader &operator=(posix_file_loader const &) = delete;
	inline constexpr posix_file_loader(posix_file_loader &&__restrict other) noexcept
		: address_begin(other.address_begin), address_end(other.address_end),
		  address_capacity(other.address_capacity)
	{
		other.address_capacity = other.address_end = other.address_begin =
			::std::bit_cast<char *>(static_cast<::std::ptrdiff_t>(-1));
	}
	inline posix_file_loader &operator=(posix_file_loader &&__restrict other) noexcept
	{
		if (__builtin_addressof(other) == this) [[unlikely]]
		{
			return *this;
		}
		::fast_io::details::posix_unload_address(address_begin,
												 static_cast<::std::size_t>(address_capacity - address_begin));
		address_begin = other.address_begin;
		address_end = other.address_end;
		address_capacity = other.address_capacity;
		other.address_capacity = other.address_end = other.address_begin =
			::std::bit_cast<char *>(static_cast<::std::ptrdiff_t>(-1));
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
	inline constexpr ::std::size_t max_size() const noexcept
	{
		return SIZE_MAX;
	}
	inline constexpr ::std::size_t capacity() const noexcept
	{
		return static_cast<::std::size_t>(this->address_capacity - this->address_begin);
	}
	inline constexpr ::std::size_t padding_size() const noexcept
	{
		return static_cast<::std::size_t>(this->address_capacity - this->address_end);
	}
	inline constexpr bool has_padding(::std::size_t n) const noexcept
	{
		return n <= this->padding_size();
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
	inline void close()
	{
		::fast_io::details::posix_unload_address(address_begin,
												 static_cast<::std::size_t>(address_capacity - address_begin));
		address_capacity = address_end = address_begin = ::std::bit_cast<char *>(static_cast<::std::ptrdiff_t>(-1));
	}
#if __has_cpp_attribute(nodiscard)
	[[nodiscard]]
#endif
	inline constexpr pointer release() noexcept
	{
		pointer temp{address_begin};
		address_capacity = address_end = address_begin = ::std::bit_cast<char *>(static_cast<::std::ptrdiff_t>(-1));
		return temp;
	}
	inline ~posix_file_loader()
	{
		::fast_io::details::posix_unload_address(address_begin,
												 static_cast<::std::size_t>(address_capacity - address_begin));
	}
};

inline constexpr basic_io_scatter_t<char> print_alias_define(io_alias_t, posix_file_loader const &load) noexcept
{
	return {load.data(), load.size()};
}

namespace freestanding
{
template <>
struct is_trivially_copyable_or_relocatable<posix_file_loader>
{
	inline static constexpr bool value = true;
};

} // namespace freestanding
} // namespace fast_io
