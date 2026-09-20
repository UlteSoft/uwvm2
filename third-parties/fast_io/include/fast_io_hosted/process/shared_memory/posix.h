#pragma once

#include "../ipc/mode.h"
#include "../ipc/posix/common.h"

#if __has_include(<sys/mman.h>)
#include <sys/mman.h>
#endif

namespace fast_io
{

namespace details
{

struct posix_shared_memory_state
{
	int fd{-1};
	::std::byte *address{};
	::std::size_t bytes{};
};

inline void posix_shared_memory_validate_mode(ipc_mode mode)
{
	constexpr auto supported_modes{
		ipc_mode::in | ipc_mode::out | ipc_mode::sync | ipc_mode::no_block |
		ipc_mode::message | ipc_mode::huge_page_2M | ipc_mode::huge_page_1G};
	if ((mode & ~supported_modes) != ipc_mode::none) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
	if ((mode & (ipc_mode::sync | ipc_mode::no_block | ipc_mode::message)) != ipc_mode::none) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
	if ((mode & (ipc_mode::huge_page_2M | ipc_mode::huge_page_1G)) ==
		(ipc_mode::huge_page_2M | ipc_mode::huge_page_1G)) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
#if !defined(__linux__)
	if ((mode & (ipc_mode::huge_page_2M | ipc_mode::huge_page_1G)) != ipc_mode::none) [[unlikely]]
	{
		throw_posix_error(ENOTSUP);
	}
#endif
}

inline int posix_shared_memory_protection(ipc_mode mode) noexcept
{
	auto const access_mode{mode & (ipc_mode::in | ipc_mode::out)};
	if (access_mode == ipc_mode::none)
	{
		return PROT_READ | PROT_WRITE;
	}
	int protection{};
	if ((access_mode & ipc_mode::in) != ipc_mode::none)
	{
		protection |= PROT_READ;
	}
	if ((access_mode & ipc_mode::out) != ipc_mode::none)
	{
		protection |= PROT_WRITE;
	}
	return protection;
}

inline int posix_shared_memory_shm_open_nothrow(
	char8_t const *name, int flags, ::mode_t permissions) noexcept
{
	return ::fast_io::noexcept_call(
		::shm_open, reinterpret_cast<char const *>(name), flags, permissions);
}

inline int posix_shared_memory_shm_unlink_nothrow(char8_t const *name) noexcept
{
	return ::fast_io::noexcept_call(
		::shm_unlink, reinterpret_cast<char const *>(name));
}

inline ::std::size_t posix_shared_memory_file_size(int fd)
{
	auto const file_status{::fast_io::status(::fast_io::basic_posix_io_observer<char>{fd})};
	if (file_status.size == 0) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
	if constexpr (sizeof(file_status.size) > sizeof(::std::size_t))
	{
		if (file_status.size > static_cast<decltype(file_status.size)>(
								   ::std::numeric_limits<::std::size_t>::max())) [[unlikely]]
		{
			throw_posix_error(EOVERFLOW);
		}
	}
	return static_cast<::std::size_t>(file_status.size);
}

struct posix_shared_memory_unlink_guard
{
	char8_t const *name{};
	bool armed{};
	~posix_shared_memory_unlink_guard()
	{
		if (armed)
		{
			posix_shared_memory_shm_unlink_nothrow(name);
		}
	}
};

inline posix_shared_memory_state posix_map_shared_memory_impl(
	int fd, ::std::size_t bytes, ipc_mode mode)
{
	posix_shared_memory_validate_mode(mode);
	if (fd == -1 || bytes == 0) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
	auto const address{::fast_io::details::sys_mmap(
		nullptr, bytes, posix_shared_memory_protection(mode), MAP_SHARED, fd, 0u)};
	return {fd, address, bytes};
}

inline int posix_create_anonymous_shared_memory_fd_fallback(ipc_mode mode)
{
	if ((mode & (ipc_mode::huge_page_2M | ipc_mode::huge_page_1G)) != ipc_mode::none)
	{
		throw_posix_error(ENOTSUP);
	}

#if defined(__FreeBSD__) && defined(SHM_ANON)
	::fast_io::posix_file file{::fast_io::noexcept_call(
		::shm_open, SHM_ANON, O_RDWR | O_CREAT,
		static_cast<::mode_t>(S_IRUSR | S_IWUSR))};
	if (!file) [[unlikely]]
	{
		throw_posix_error();
	}
	posix_ipc_set_close_on_exec(file.native_handle());
	return file.release();
#else
	static ::std::atomic_size_t sequence{};
	for (;;)
	{
		char8_t seed[64]{};
		auto iter{posix_ipc_print_unsigned(
			seed, static_cast<::std::uintmax_t>(::getpid()))};
		*iter++ = u8'_';
		iter = posix_ipc_print_unsigned(iter,
										static_cast<::std::uintmax_t>(
											sequence.fetch_add(1u, ::std::memory_order_relaxed)));
		char8_t temporary_name[32]{};
		constexpr char8_t prefix[]{u8"/fio_a_"};
		auto name_iter{temporary_name};
		for (::std::size_t i{}; i != sizeof(prefix) - 1u; ++i)
		{
			*name_iter++ = prefix[i];
		}
		name_iter = posix_ipc_print_hash_token(name_iter,
											   posix_ipc_hash_name(seed, static_cast<::std::size_t>(iter - seed),
																   0x616e6f6e5f73686dULL));
		*name_iter = u8'\0';
		int flags{O_RDWR | O_CREAT | O_EXCL};
		::fast_io::posix_file file{posix_shared_memory_shm_open_nothrow(
			temporary_name, flags, S_IRUSR | S_IWUSR)};
		if (file)
		{
			posix_shared_memory_unlink_guard unlink_guard{temporary_name, true};
			posix_ipc_set_close_on_exec(file.native_handle());
			if (posix_shared_memory_shm_unlink_nothrow(temporary_name) == -1) [[unlikely]]
			{
				throw_posix_error();
			}
			unlink_guard.armed = false;
			return file.release();
		}
		if (errno != EEXIST) [[unlikely]]
		{
			throw_posix_error();
		}
	}
#endif
}

inline int posix_create_anonymous_shared_memory_fd(ipc_mode mode)
{
#if defined(__linux__) && defined(__NR_memfd_create)
	unsigned flags{0x0001u}; // MFD_CLOEXEC
	if ((mode & (ipc_mode::huge_page_2M | ipc_mode::huge_page_1G)) != ipc_mode::none)
	{
		flags |= 0x0004u; // MFD_HUGETLB
		flags |= ((mode & ipc_mode::huge_page_1G) != ipc_mode::none ? 30u : 21u) << 26u;
	}
	int fd{::fast_io::system_call<__NR_memfd_create, int>(
		u8"fast_io_shared_memory", flags)};
	if (!::fast_io::linux_system_call_fails(fd))
	{
		return fd;
	}
	if (-fd != ENOSYS) [[unlikely]]
	{
		::fast_io::system_call_throw_error(fd);
	}
#endif
	return posix_create_anonymous_shared_memory_fd_fallback(mode);
}

inline posix_shared_memory_state posix_create_shared_memory_impl(
	::std::size_t bytes, ipc_mode mode)
{
	posix_shared_memory_validate_mode(mode);
	if (bytes == 0) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
	::fast_io::posix_file file{posix_create_anonymous_shared_memory_fd(mode)};
	::fast_io::truncate(::fast_io::basic_posix_io_observer<char>{file.native_handle()}, bytes);
	auto state{posix_map_shared_memory_impl(file.native_handle(), bytes, mode)};
	file.release();
	return state;
}

struct posix_shared_memory_name
{
	char8_t object_name[32]{};
	char8_t lock_path[128]{};
};

inline posix_shared_memory_name posix_shared_memory_make_name(
	char8_t const *name, ::std::size_t name_size)
{
	posix_ipc_validate_name(name, name_size);
	constexpr ::std::uint64_t domain{0x6e616d65645f7368ULL};
	posix_shared_memory_name result{};
	constexpr char8_t prefix[]{u8"/fio_s_"};
	auto iter{result.object_name};
	for (::std::size_t i{}; i != sizeof(prefix) - 1u; ++i)
	{
		*iter++ = prefix[i];
	}
	iter = posix_ipc_print_hash_token(iter,
									  posix_ipc_hash_name(name, name_size, domain));
	*iter = u8'\0';
	constexpr char8_t lock_suffix[]{u8".shm.lock"};
	posix_ipc_make_user_path(
		result.lock_path, result.lock_path + sizeof(result.lock_path),
		name, name_size, domain, lock_suffix, sizeof(lock_suffix) - 1u);
	return result;
}

inline posix_shared_memory_state posix_create_named_shared_memory_impl(
	char8_t const *name, ::std::size_t name_size, ::std::size_t bytes, ipc_mode mode)
{
	posix_shared_memory_validate_mode(mode);
	if (bytes == 0) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
	if ((mode & (ipc_mode::huge_page_2M | ipc_mode::huge_page_1G)) != ipc_mode::none) [[unlikely]]
	{
		// POSIX shm objects are not guaranteed to be hugetlbfs-backed. Refuse
		// the request instead of silently returning ordinary pages.
		throw_posix_error(ENOTSUP);
	}
	auto const shared_name{posix_shared_memory_make_name(name, name_size)};
	posix_ipc_file_lock initialization_lock{shared_name.lock_path, false};
	for (;;)
	{
		bool created{};
		int fd{posix_shared_memory_shm_open_nothrow(
			shared_name.object_name, O_RDWR | O_CREAT | O_EXCL,
			S_IRUSR | S_IWUSR)};
		if (fd != -1)
		{
			created = true;
		}
		else
		{
			if (errno != EEXIST) [[unlikely]]
			{
				throw_posix_error();
			}
			auto const access_mode{mode & (ipc_mode::in | ipc_mode::out)};
			int const open_flags{access_mode == ipc_mode::in ? O_RDONLY : O_RDWR};
			fd = posix_shared_memory_shm_open_nothrow(
				shared_name.object_name, open_flags, 0);
			if (fd == -1)
			{
				if (errno == ENOENT)
				{
					continue;
				}
				throw_posix_error();
			}
		}
		::fast_io::posix_file file{fd};
		posix_shared_memory_unlink_guard unlink_guard{shared_name.object_name, created};
			posix_ipc_set_close_on_exec(file.native_handle());
			struct ::stat status{};
			if (::fast_io::noexcept_call(::fstat, file.native_handle(),
										 __builtin_addressof(status)) == -1) [[unlikely]]
			{
				throw_posix_error();
			}
			if (status.st_uid != ::geteuid()) [[unlikely]]
			{
				throw_posix_error(EACCES);
			}
			if (!created && status.st_size == 0)
			{
				// A creator may have terminated between shm_open and ftruncate.
				// Under the per-name lock, a zero-sized object cannot be a
				// successfully constructed fast_io shared-memory object.
				if (posix_shared_memory_shm_unlink_nothrow(
						shared_name.object_name) == -1 &&
					errno != ENOENT) [[unlikely]]
				{
					throw_posix_error();
				}
				continue;
			}
			if (created)
			{
				if (::fast_io::noexcept_call(::fchmod, file.native_handle(),
											 static_cast<::mode_t>(S_IRUSR | S_IWUSR)) == -1) [[unlikely]]
				{
					throw_posix_error();
				}
				::fast_io::truncate(::fast_io::basic_posix_io_observer<char>{file.native_handle()}, bytes);
			}
			auto const actual_bytes{posix_shared_memory_file_size(file.native_handle())};
			auto state{posix_map_shared_memory_impl(file.native_handle(), actual_bytes, mode)};
			file.release();
			unlink_guard.armed = false;
			return state;
	}
}

struct posix_create_named_shared_memory_parameter
{
	::std::size_t bytes{};
	ipc_mode mode{};
	inline posix_shared_memory_state operator()(
		char8_t const *name, ::std::size_t name_size) const
	{
		return posix_create_named_shared_memory_impl(name, name_size, bytes, mode);
	}
};

template <typename T>
	requires(::fast_io::constructible_to_os_c_str<T>)
inline posix_shared_memory_state posix_create_named_shared_memory_impl(
	T const &name, ::std::size_t bytes, ipc_mode mode)
{
	return ::fast_io::details::posix_ipc_u8_api_common(
		name, posix_create_named_shared_memory_parameter{bytes, mode});
}

inline posix_shared_memory_state posix_duplicate_shared_memory_impl(
	int fd, ::std::size_t bytes, ipc_mode mode)
{
	::fast_io::posix_file duplicated_file{posix_ipc_duplicate_close_on_exec(fd)};
	auto state{posix_map_shared_memory_impl(duplicated_file.native_handle(), bytes, mode)};
	duplicated_file.release();
	return state;
}

inline posix_shared_memory_state posix_adopt_shared_memory_impl(
	int fd, ::std::size_t requested_bytes, ipc_mode mode)
{
	posix_shared_memory_validate_mode(mode);
	if (fd == -1) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
	::fast_io::posix_file file{fd};
	posix_ipc_set_close_on_exec(file.native_handle());
	auto const actual_bytes{posix_shared_memory_file_size(file.native_handle())};
	auto const bytes{requested_bytes == 0 ? actual_bytes : requested_bytes};
	if (bytes > actual_bytes) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
	auto state{posix_map_shared_memory_impl(file.native_handle(), bytes, mode)};
	file.release();
	return state;
}

inline void posix_unlink_shared_memory_impl(
	char8_t const *name, ::std::size_t name_size)
{
	auto const shared_name{posix_shared_memory_make_name(name, name_size)};
	posix_ipc_file_lock initialization_lock{shared_name.lock_path, false};
	if (posix_shared_memory_shm_unlink_nothrow(
			shared_name.object_name) == -1) [[unlikely]]
	{
		throw_posix_error();
	}
}

inline int posix_shared_memory_close_state_nothrow(
	int fd, ::std::byte *address, ::std::size_t bytes) noexcept
{
	int first_error{};
	if (address != nullptr)
	{
		auto const result{::fast_io::details::sys_munmap_nothrow(address, bytes)};
#if defined(__linux__) && defined(__NR_munmap)
		if (::fast_io::linux_system_call_fails(result))
		{
			first_error = -result;
		}
#else
		if (result == -1)
		{
			first_error = errno;
		}
#endif
	}
	if (fd != -1)
	{
		auto const result{::fast_io::details::sys_close(fd)};
#if defined(__linux__) && defined(__NR_close)
		if (first_error == 0 && ::fast_io::linux_system_call_fails(result))
		{
			first_error = -result;
		}
#else
		if (first_error == 0 && result == -1)
		{
			first_error = errno;
		}
#endif
	}
	return first_error;
}

} // namespace details

class posix_shared_memory
{
public:
	using native_handle_type = int;
	using value_type = ::std::byte;
	using pointer = value_type *;
	using const_pointer = value_type const *;
	using size_type = ::std::size_t;

	native_handle_type fd{-1};
	pointer address{};
	size_type bytes{};

	inline constexpr posix_shared_memory() noexcept = default;

	inline explicit posix_shared_memory(
		size_type size, ipc_mode requested_mode = ipc_mode::in | ipc_mode::out)
	{
		assign(::fast_io::details::posix_create_shared_memory_impl(size, requested_mode), requested_mode);
	}

	template <::fast_io::constructible_to_os_c_str T>
	inline posix_shared_memory(
		T const &name, size_type size, ipc_mode requested_mode = ipc_mode::in | ipc_mode::out)
	{
		assign(::fast_io::details::posix_create_named_shared_memory_impl(name, size, requested_mode), requested_mode);
	}

	inline explicit posix_shared_memory(
		io_construct_t, native_handle_type handle, ipc_mode requested_mode = ipc_mode::in | ipc_mode::out)
	{
		assign(::fast_io::details::posix_adopt_shared_memory_impl(handle, 0u, requested_mode), requested_mode);
	}

	inline posix_shared_memory(
		io_construct_t, native_handle_type handle, size_type size, ipc_mode requested_mode)
	{
		assign(::fast_io::details::posix_adopt_shared_memory_impl(handle, size, requested_mode), requested_mode);
	}

	inline posix_shared_memory(posix_shared_memory const &other)
		: mapping_mode{other.mapping_mode}
	{
		if (other.fd != -1) [[likely]]
		{
			assign(::fast_io::details::posix_duplicate_shared_memory_impl(
					   other.fd, other.bytes, other.mapping_mode),
				   other.mapping_mode);
		}
	}

	inline posix_shared_memory &operator=(posix_shared_memory const &other)
	{
		if (__builtin_addressof(other) == this) [[unlikely]]
		{
			return *this;
		}
		posix_shared_memory temp{other};
		swap(temp);
		return *this;
	}

	inline posix_shared_memory(posix_shared_memory &&other) noexcept
		: fd{other.fd}, address{other.address}, bytes{other.bytes}, mapping_mode{other.mapping_mode}
	{
		other.fd = -1;
		other.address = nullptr;
		other.bytes = 0;
	}

	inline posix_shared_memory &operator=(posix_shared_memory &&other) noexcept
	{
		if (__builtin_addressof(other) != this) [[likely]]
		{
			reset();
			fd = other.fd;
			address = other.address;
			bytes = other.bytes;
			mapping_mode = other.mapping_mode;
			other.fd = -1;
			other.address = nullptr;
			other.bytes = 0;
		}
		return *this;
	}

	inline constexpr native_handle_type native_handle() const noexcept
	{
		return fd;
	}

	inline explicit constexpr operator bool() const noexcept
	{
		return fd != -1;
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

	inline void swap(posix_shared_memory &other) noexcept
	{
		auto const old_fd{fd};
		auto const old_address{address};
		auto const old_bytes{bytes};
		auto const old_mode{mapping_mode};
		fd = other.fd;
		address = other.address;
		bytes = other.bytes;
		mapping_mode = other.mapping_mode;
		other.fd = old_fd;
		other.address = old_address;
		other.bytes = old_bytes;
		other.mapping_mode = old_mode;
	}

	inline void reset() noexcept
	{
		::fast_io::details::posix_shared_memory_close_state_nothrow(fd, address, bytes);
		fd = -1;
		address = nullptr;
		bytes = 0;
	}

	inline void close()
	{
		auto const error{
			::fast_io::details::posix_shared_memory_close_state_nothrow(fd, address, bytes)};
		fd = -1;
		address = nullptr;
		bytes = 0;
		if (error != 0) [[unlikely]]
		{
			throw_posix_error(error);
		}
	}

	inline ~posix_shared_memory()
	{
		reset();
	}

private:
	ipc_mode mapping_mode{ipc_mode::in | ipc_mode::out};

	inline void assign(::fast_io::details::posix_shared_memory_state state, ipc_mode new_mode) noexcept
	{
		fd = state.fd;
		address = state.address;
		bytes = state.bytes;
		mapping_mode = new_mode;
	}
};

template <::fast_io::constructible_to_os_c_str T>
inline void posix_unlink_shared_memory(T const &name)
{
	::fast_io::details::posix_ipc_u8_api_common(
		name, [](char8_t const *native_name, ::std::size_t native_name_size) {
			::fast_io::details::posix_unlink_shared_memory_impl(
				native_name, native_name_size);
		});
}

namespace freestanding
{

template <>
struct is_trivially_copyable_or_relocatable<::fast_io::posix_shared_memory>
{
	inline static constexpr bool value = true;
};

} // namespace freestanding

} // namespace fast_io
