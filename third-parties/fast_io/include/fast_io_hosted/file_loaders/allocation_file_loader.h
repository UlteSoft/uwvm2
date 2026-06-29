#pragma once

namespace fast_io
{

namespace details
{

struct allocation_file_loader_closer_impl
{
	int fd{-1};
	char *address_begin{};
	inline explicit constexpr allocation_file_loader_closer_impl(int fdd, char *addressbegin)
		: fd{fdd}, address_begin{addressbegin}
	{
	}
	inline allocation_file_loader_closer_impl(allocation_file_loader_closer_impl const &) = delete;
	inline allocation_file_loader_closer_impl &operator=(allocation_file_loader_closer_impl const &) = delete;
	inline ~allocation_file_loader_closer_impl()
	{
		if (fd != -1)
		{
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__)
			::fast_io::noexcept_call(::_close, fd);
#else
			::fast_io::noexcept_call(::close, fd);
#endif
		}
#if FAST_IO_HAS_BUILTIN(__builtin_free)
		__builtin_free(address_begin);
#else
		::std::free(address_begin);
#endif
	}
};

inline void close_allocation_file_loader_impl(int fd, char *address_begin, char *address_end) noexcept
{
	allocation_file_loader_closer_impl closer(fd, address_begin);
	if (fd == -1)
	{
		return;
	}
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__)
	constexpr bool needshrinktoint32{true};
#else
	constexpr bool needshrinktoint32{};
#endif
	using towritetype = ::std::conditional_t<needshrinktoint32, unsigned, ::std::size_t>;
	while (address_begin != address_end)
	{
		::std::size_t towrite{static_cast<::std::size_t>(address_end - address_begin)};
		if constexpr (sizeof(towritetype) < sizeof(::std::size_t))
		{
			constexpr ::std::size_t towritemx{::std::numeric_limits<towritetype>::max()};
			if (towritemx < towrite)
			{
				towrite = towritemx;
			}
		}
		auto ret{::fast_io::noexcept_call(
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__)
			::_write
#else
			::write
#endif
			,
			fd, address_begin, static_cast<towritetype>(towrite))};
		if (ret == -1)
		{
#ifdef EAGAIN
			if (errno != EAGAIN)
			{
				break;
			}
#else
			break;
#endif
		}
		address_begin += ret;
	}
}

struct passparm
{
	int fd;
	bool writeback;
};

struct allocation_file_loader_ret
{
	char *address_begin;
	char *address_end;
	char *address_capacity;
	int fd;
};

struct load_file_allocation_guard
{
	void *address{};
	inline explicit constexpr load_file_allocation_guard() noexcept = default;
	inline explicit load_file_allocation_guard(::std::size_t file_size)
		: address(file_size == 0 ? nullptr :
#if FAST_IO_HAS_BUILTIN(__builtin_malloc)
			  __builtin_malloc
#else
			  ::std::malloc
#endif
			  (file_size))
	{
		if (file_size != 0 && address == nullptr)
		{
			throw_posix_error(ENOMEM);
		}
	}
	inline load_file_allocation_guard(load_file_allocation_guard const &) = delete;
	inline load_file_allocation_guard &operator=(load_file_allocation_guard const &) = delete;
	inline ~load_file_allocation_guard()
	{
#if FAST_IO_HAS_BUILTIN(__builtin_free)
		__builtin_free(address);
#else
		::std::free(address);
#endif
	}
};

inline ::std::size_t allocation_file_loader_padded_capacity(::std::size_t file_size,
															::std::size_t extra_bytes)
{
	if (SIZE_MAX - file_size < extra_bytes)
	{
		throw_posix_error(EINVAL);
	}
	return file_size + extra_bytes;
}

inline allocation_file_loader_ret allocation_load_address_options_impl(int fd,
																	   ::fast_io::allocation_mmap_options options)
{
	::std::size_t filesize{::fast_io::details::posix_loader_get_file_size(fd)};
	::std::size_t capacity{allocation_file_loader_padded_capacity(filesize, options.extra_bytes)};
	load_file_allocation_guard guard{capacity};
	posix_io_observer piob{fd};
	auto addr{reinterpret_cast<char *>(guard.address)};
	auto addr_ed{addr == nullptr ? nullptr : addr + filesize};
	auto addr_cap{addr == nullptr ? nullptr : addr + capacity};
	if (filesize)
	{
		::fast_io::operations::decay::read_all_bytes_decay(piob, reinterpret_cast<::std::byte *>(addr),
														   reinterpret_cast<::std::byte *>(addr_ed));
	}
	if (options.padding_mode == ::fast_io::file_loader_padding_mode::zero && options.extra_bytes)
	{
		::fast_io::freestanding::my_memset(addr_ed, 0, options.extra_bytes);
	}
	guard.address = nullptr;
	return {addr, addr_ed, addr_cap, -1};
}

inline allocation_file_loader_ret allocation_load_address_impl(int fd)
{
	return allocation_load_address_options_impl(fd, ::fast_io::allocation_mmap_options{});
}

inline void rewind_allocation_file_loader(int fd)
{
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__)
	auto seekret = ::fast_io::noexcept_call(::_lseeki64, fd, 0, 0);
#else
#if defined(_LARGEFILE64_SOURCE)
	auto seekret = ::fast_io::noexcept_call(::lseek64, fd, 0, 0);
#else
	auto seekret = ::fast_io::noexcept_call(::lseek, fd, 0, 0);
#endif
#endif
	if (seekret == -1)
	{
		throw_posix_error();
	}
}

template <typename... Args>
inline allocation_file_loader_ret allocation_load_file_impl(::fast_io::allocation_mmap_options options, Args &&...args)
{
	::fast_io::posix_file pf(::fast_io::freestanding::forward<Args>(args)...);
	auto ret{allocation_load_address_options_impl(pf.fd, options)};

	if (options.write_back)
	{
		load_file_allocation_guard loader;
		loader.address = ret.address_begin;
		rewind_allocation_file_loader(pf.fd);
		loader.address = nullptr;
		ret.fd = pf.release();
	}
	return ret;
}

template <typename... Args>
inline allocation_file_loader_ret allocation_load_file_impl(bool writeback, Args &&...args)
{
	::fast_io::allocation_mmap_options options;
	options.write_back = writeback;
	return allocation_load_file_impl(options, ::fast_io::freestanding::forward<Args>(args)...);
}

inline allocation_file_loader_ret allocation_load_file_fd_impl(::fast_io::allocation_mmap_options options, int fd)
{
	::fast_io::posix_file pf(::fast_io::io_dup, ::fast_io::posix_io_observer{fd});
	rewind_allocation_file_loader(pf.fd);
	auto ret{allocation_load_address_options_impl(pf.fd, options)};
	if (options.write_back)
	{
		load_file_allocation_guard loader;
		loader.address = ret.address_begin;
		rewind_allocation_file_loader(pf.fd);
		loader.address = nullptr;
		ret.fd = pf.release();
	}
	return ret;
}

inline allocation_file_loader_ret allocation_load_file_fd_impl(bool writeback, int fd)
{
	::fast_io::allocation_mmap_options options;
	options.write_back = writeback;
	return allocation_load_file_fd_impl(options, fd);
}

} // namespace details

struct released_allocation_file_loader_mapping
{
	char *address_begin{};
	char *address_end{};
	char *address_capacity{};
	int fd{-1};
};

class allocation_file_loader
{
public:
	using file_type = posix_file;
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
	using native_handle_type = released_allocation_file_loader_mapping;

	native_handle_type storage{};
	inline explicit constexpr allocation_file_loader() noexcept = default;
	inline explicit constexpr allocation_file_loader(native_handle_type mapping) noexcept
		: storage(mapping)
	{
	}

	inline explicit allocation_file_loader(posix_at_entry pate)
	{
		auto ret{::fast_io::details::allocation_load_file_fd_impl(false, pate.fd)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.fd};
	}
	inline explicit allocation_file_loader(native_fs_dirent fsdirent, open_mode om = open_mode::in,
										   perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::details::allocation_load_file_impl(false, fsdirent, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.fd};
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit allocation_file_loader(T const &filename, open_mode om = open_mode::in,
										   perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::details::allocation_load_file_impl(false, filename, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.fd};
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit allocation_file_loader(native_at_entry ent, T const &filename, open_mode om = open_mode::in,
										   perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::details::allocation_load_file_impl(false, ent, filename, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.fd};
	}
	inline explicit allocation_file_loader(allocation_mmap_options options, posix_at_entry pate)
	{
		auto ret{::fast_io::details::allocation_load_file_fd_impl(options, pate.fd)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.fd};
	}
	inline explicit allocation_file_loader(allocation_mmap_options options, native_fs_dirent fsdirent,
										   open_mode om = open_mode::in, perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::details::allocation_load_file_impl(options, fsdirent, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.fd};
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit allocation_file_loader(allocation_mmap_options options, T const &filename,
										   open_mode om = open_mode::in, perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::details::allocation_load_file_impl(options, filename, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.fd};
	}
	template <::fast_io::constructible_to_os_c_str T>
	inline explicit allocation_file_loader(allocation_mmap_options options, native_at_entry ent, T const &filename,
										   open_mode om = open_mode::in, perms pm = static_cast<perms>(436))
	{
		auto ret{::fast_io::details::allocation_load_file_impl(options, ent, filename, om, pm)};
		storage = native_handle_type{ret.address_begin, ret.address_end, ret.address_capacity, ret.fd};
	}

	inline allocation_file_loader(allocation_file_loader const &) = delete;
	inline allocation_file_loader &operator=(allocation_file_loader const &) = delete;
	inline constexpr allocation_file_loader(allocation_file_loader &&__restrict other) noexcept
		: storage(other.storage)
	{
		other.storage = {};
	}
	inline allocation_file_loader &operator=(allocation_file_loader &&__restrict other) noexcept
	{
		if (__builtin_addressof(other) == this) [[unlikely]]
		{
			return *this;
		}

		::fast_io::details::close_allocation_file_loader_impl(storage.fd, storage.address_begin, storage.address_end);

		// There is no need to check the 'this' pointer as there are no side effects
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
	inline constexpr ::std::size_t max_size() const noexcept
	{
		return SIZE_MAX;
	}
	inline constexpr ::std::size_t capacity() const noexcept
	{
		return static_cast<::std::size_t>(this->storage.address_capacity - this->storage.address_begin);
	}
	inline constexpr ::std::size_t padding_size() const noexcept
	{
		return static_cast<::std::size_t>(this->storage.address_capacity - this->storage.address_end);
	}
	inline constexpr bool has_padding(::std::size_t n) const noexcept
	{
		return n <= this->padding_size();
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
	inline void close()
	{
		::fast_io::details::close_allocation_file_loader_impl(storage.fd, storage.address_begin, storage.address_end);
		storage = {};
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
	inline ~allocation_file_loader()
	{
		::fast_io::details::close_allocation_file_loader_impl(storage.fd, storage.address_begin, storage.address_end);
	}
};

inline constexpr basic_io_scatter_t<char> print_alias_define(io_alias_t, allocation_file_loader const &load) noexcept
{
	return {load.data(), load.size()};
}

} // namespace fast_io
