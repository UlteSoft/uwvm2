#pragma once

#if __has_include(<sys/file.h>)
#include <sys/file.h>
#endif
#if __has_include(<sys/stat.h>)
#include <sys/stat.h>
#endif
#include <atomic>

namespace fast_io::details
{

using posix_ipc_u8_tlc_str =
	::fast_io::containers::basic_string<char8_t, ::fast_io::native_thread_local_allocator>;

inline void posix_ipc_validate_name(
	char8_t const *name, ::std::size_t name_size)
{
	if (name == nullptr || name_size == 0) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
}

template <typename T, typename Func>
	requires(::fast_io::constructible_to_os_c_str<T>)
inline auto posix_ipc_u8_api_common(T const &name, Func callback)
{
	if constexpr (::fast_io::type_has_c_str_method<T>)
	{
		if (name.c_str() == nullptr) [[unlikely]]
		{
			throw_posix_error(EINVAL);
		}
	}
	else if constexpr (!::std::is_array_v<T>)
	{
		if (name.data() == nullptr) [[unlikely]]
		{
			throw_posix_error(EINVAL);
		}
	}
	return ::fast_io::posix_api_common(
		name, [&](char const *native_name, ::std::size_t native_name_size) {
			// Preserve the POSIX byte sequence verbatim. The copy establishes
			// char8_t storage; it does not validate or normalize UTF-8.
			posix_ipc_u8_tlc_str byte_name;
			byte_name.resize(native_name_size);
			if (native_name_size != 0)
			{
				__builtin_memcpy(byte_name.data(), native_name, native_name_size);
			}
			posix_ipc_validate_name(byte_name.c_str(), byte_name.size());
			return callback(byte_name.c_str(), byte_name.size());
		});
}

inline char8_t *posix_ipc_print_unsigned(
	char8_t *iter, ::std::uintmax_t value) noexcept
{
	char8_t buffer[::std::numeric_limits<::std::uintmax_t>::digits10 + 1u];
	auto curr{buffer + sizeof(buffer)};
	do
	{
		auto const quotient{value / 10u};
		*--curr = static_cast<char8_t>(u8'0' + static_cast<unsigned>(value - quotient * 10u));
		value = quotient;
	} while (value != 0);
	for (; curr != buffer + sizeof(buffer); ++curr)
	{
		*iter++ = *curr;
	}
	return iter;
}

struct posix_ipc_hash128
{
	::std::uint64_t first{};
	::std::uint64_t second{};
};

inline constexpr ::std::uint64_t posix_ipc_hash_avalanche(
	::std::uint64_t value) noexcept
{
	value ^= value >> 30u;
	value *= 0xbf58476d1ce4e5b9ULL;
	value ^= value >> 27u;
	value *= 0x94d049bb133111ebULL;
	return value ^ (value >> 31u);
}

inline posix_ipc_hash128 posix_ipc_hash_name(
	char8_t const *name, ::std::size_t name_size,
	::std::uint64_t domain) noexcept
{
	::std::uint64_t first{0xcbf29ce484222325ULL ^ domain};
	::std::uint64_t second{0x9e3779b97f4a7c15ULL ^ (domain << 1u)};
	auto uid{static_cast<::std::uintmax_t>(::geteuid())};
	for (unsigned shift{}; shift != sizeof(uid) * 8u; shift += 8u)
	{
		auto const byte{static_cast<unsigned char>(uid >> shift)};
		first = (first ^ byte) * 0x100000001b3ULL;
		second = ((second ^ static_cast<unsigned char>(byte + 0x9du)) << 13u |
				  (second ^ static_cast<unsigned char>(byte + 0x9du)) >> 51u) *
				 0x9e3779b185ebca87ULL;
	}
	auto const bytes{reinterpret_cast<unsigned char const *>(name)};
	for (::std::size_t i{}; i != name_size; ++i)
	{
		auto const byte{bytes[i]};
		first = (first ^ byte) * 0x100000001b3ULL;
		auto mixed{second ^ static_cast<unsigned char>(byte + 0x5bu)};
		second = ((mixed << 17u) | (mixed >> 47u)) * 0xc2b2ae3d27d4eb4fULL;
	}
	first ^= static_cast<::std::uint64_t>(name_size);
	second ^= static_cast<::std::uint64_t>(name_size) << 32u;
	return {posix_ipc_hash_avalanche(first ^ second),
			posix_ipc_hash_avalanche(second ^ (first << 1u))};
}

inline char8_t *posix_ipc_print_hash_token(
	char8_t *iter, posix_ipc_hash128 hash) noexcept
{
	constexpr char8_t alphabet[]{
		u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"};
	unsigned char bytes[16];
	for (unsigned i{}; i != 8u; ++i)
	{
		bytes[i] = static_cast<unsigned char>(hash.first >> ((7u - i) * 8u));
		bytes[8u + i] = static_cast<unsigned char>(hash.second >> ((7u - i) * 8u));
	}
	::std::uint_least32_t accumulator{};
	unsigned bits{};
	for (auto const byte : bytes)
	{
		accumulator = static_cast<::std::uint_least32_t>((accumulator << 8u) | byte);
		bits += 8u;
		while (bits >= 6u)
		{
			bits -= 6u;
			*iter++ = alphabet[(accumulator >> bits) & 0x3fu];
		}
	}
	if (bits != 0u)
	{
		*iter++ = alphabet[(accumulator << (6u - bits)) & 0x3fu];
	}
	return iter;
}

inline void posix_ipc_set_close_on_exec(int fd)
{
#if defined(F_GETFD) && defined(F_SETFD) && defined(FD_CLOEXEC)
	auto const flags{::fast_io::details::sys_fcntl(fd, F_GETFD)};
	if ((flags & FD_CLOEXEC) == 0)
	{
		::fast_io::details::sys_fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
	}
#else
	(void)fd;
#endif
}

inline char8_t *posix_ipc_user_directory(
	char8_t *first, char8_t *last)
{
	constexpr char8_t prefix[]{u8"/tmp/fast_io_ipc_"};
	constexpr auto maximum_uid_digits{
		::std::numeric_limits<::std::uintmax_t>::digits10 + 1u};
	constexpr auto required_capacity{
		sizeof(prefix) - 1u + maximum_uid_digits + 1u};
	if (static_cast<::std::size_t>(last - first) < required_capacity) [[unlikely]]
	{
		throw_posix_error(ENAMETOOLONG);
	}
	auto iter{first};
	for (auto ch : prefix)
	{
		if (ch == u8'\0')
		{
			break;
		}
		*iter++ = ch;
	}
	iter = posix_ipc_print_unsigned(
		iter, static_cast<::std::uintmax_t>(::geteuid()));
	if (iter == last) [[unlikely]]
	{
		throw_posix_error(ENAMETOOLONG);
	}
	*iter = u8'\0';
	auto const path{reinterpret_cast<char const *>(first)};
	if (::fast_io::noexcept_call(::mkdir, path,
								 static_cast<::mode_t>(S_IRWXU)) == -1 &&
		errno != EEXIST) [[unlikely]]
	{
		throw_posix_error();
	}
	int directory_flags{O_RDONLY};
#ifdef O_DIRECTORY
	directory_flags |= O_DIRECTORY;
#endif
#ifdef O_CLOEXEC
	directory_flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
	directory_flags |= O_NOFOLLOW;
#endif
	int directory_fd{::open(path, directory_flags, 0)};
#ifdef O_CLOEXEC
	if (directory_fd == -1 && errno == EINVAL &&
		(directory_flags & O_CLOEXEC) != 0)
	{
		directory_flags &= ~O_CLOEXEC;
		directory_fd = ::open(path, directory_flags, 0);
	}
#endif
	if (directory_fd == -1) [[unlikely]]
	{
		throw_posix_error();
	}
	::fast_io::posix_file directory{directory_fd};
	posix_ipc_set_close_on_exec(directory.native_handle());
	struct ::stat status{};
	if (::fast_io::noexcept_call(::fstat, directory.native_handle(),
								 __builtin_addressof(status)) == -1) [[unlikely]]
	{
		throw_posix_error();
	}
	if (!S_ISDIR(status.st_mode) || status.st_uid != ::geteuid()) [[unlikely]]
	{
		throw_posix_error(EACCES);
	}
	if ((status.st_mode & static_cast<::mode_t>(S_IRWXU | S_IRWXG | S_IRWXO)) !=
			static_cast<::mode_t>(S_IRWXU) &&
		::fast_io::noexcept_call(::fchmod, directory.native_handle(),
								 static_cast<::mode_t>(S_IRWXU)) == -1) [[unlikely]]
	{
		throw_posix_error();
	}
	return iter;
}

inline char8_t *posix_ipc_make_user_path(
	char8_t *first, char8_t *last, char8_t const *name,
	::std::size_t name_size, ::std::uint64_t domain,
	char8_t const *suffix, ::std::size_t suffix_size)
{
	auto iter{posix_ipc_user_directory(first, last)};
	auto const required{1u + 22u + suffix_size + 1u};
	if (static_cast<::std::size_t>(last - iter) < required) [[unlikely]]
	{
		throw_posix_error(ENAMETOOLONG);
	}
	*iter++ = u8'/';
	iter = posix_ipc_print_hash_token(iter,
									  posix_ipc_hash_name(name, name_size, domain));
	for (::std::size_t i{}; i != suffix_size; ++i)
	{
		*iter++ = suffix[i];
	}
	*iter = u8'\0';
	return iter;
}

inline int posix_ipc_duplicate_close_on_exec(int old_fd)
{
	int fd{-1};
#ifdef F_DUPFD_CLOEXEC
	fd = ::fcntl(old_fd, F_DUPFD_CLOEXEC, 0);
	if (fd != -1)
	{
		return fd;
	}
	if (errno != EINVAL) [[unlikely]]
	{
		throw_posix_error();
	}
#endif
	::fast_io::posix_file duplicated{::fast_io::details::sys_dup(old_fd)};
	posix_ipc_set_close_on_exec(duplicated.native_handle());
	return duplicated.release();
}

inline int posix_ipc_close_nothrow(int fd) noexcept
{
	if (fd == -1)
	{
		return 0;
	}
	auto const result{::fast_io::details::sys_close(fd)};
#if defined(__linux__) && defined(__NR_close)
	if (::fast_io::linux_system_call_fails(result))
	{
		return -result;
	}
#else
	if (result == -1)
	{
		return errno;
	}
#endif
	return 0;
}

class posix_ipc_file_lock
{
public:
	int fd{-1};

	inline constexpr posix_ipc_file_lock() noexcept = default;

	inline explicit posix_ipc_file_lock(
		char8_t const *path, bool nonblocking)
	{
		int flags{O_RDWR | O_CREAT};
#ifdef O_CLOEXEC
		flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
		flags |= O_NOFOLLOW;
#endif
		fd = ::open(
			reinterpret_cast<char const *>(path), flags,
			static_cast<::mode_t>(S_IRUSR | S_IWUSR));
#ifdef O_CLOEXEC
		if (fd == -1 && errno == EINVAL && (flags & O_CLOEXEC) != 0)
		{
			flags &= ~O_CLOEXEC;
			fd = ::open(
				reinterpret_cast<char const *>(path), flags,
				static_cast<::mode_t>(S_IRUSR | S_IWUSR));
		}
#endif
		if (fd == -1) [[unlikely]]
		{
			throw_posix_error();
		}
		::fast_io::posix_file file{fd};
		fd = -1;
		{
			posix_ipc_set_close_on_exec(file.native_handle());
			struct ::stat status{};
			if (::fast_io::noexcept_call(::fstat, file.native_handle(),
										 __builtin_addressof(status)) == -1) [[unlikely]]
			{
				throw_posix_error();
			}
			if (!S_ISREG(status.st_mode) || status.st_uid != ::geteuid()) [[unlikely]]
			{
				throw_posix_error(EACCES);
			}
			if (::fast_io::noexcept_call(::fchmod, file.native_handle(),
										 static_cast<::mode_t>(S_IRUSR | S_IWUSR)) == -1) [[unlikely]]
			{
				throw_posix_error();
			}
			lock(nonblocking);
		}
		fd = file.release();
	}

	posix_ipc_file_lock(posix_ipc_file_lock const &) = delete;
	posix_ipc_file_lock &operator=(posix_ipc_file_lock const &) = delete;

	inline constexpr posix_ipc_file_lock(
		posix_ipc_file_lock &&other) noexcept : fd{other.fd}
	{
		other.fd = -1;
	}

	inline constexpr posix_ipc_file_lock &operator=(
		posix_ipc_file_lock &&other) noexcept
	{
		if (__builtin_addressof(other) != this)
		{
			if (fd != -1)
			{
				::fast_io::details::sys_close(fd);
			}
			fd = other.fd;
			other.fd = -1;
		}
		return *this;
	}

	inline constexpr int release() noexcept
	{
		auto const result{fd};
		fd = -1;
		return result;
	}

	inline ~posix_ipc_file_lock()
	{
		if (fd != -1)
		{
			::fast_io::details::sys_close(fd);
		}
	}

private:
	inline void lock(bool nonblocking)
	{
#if defined(LOCK_EX) && defined(LOCK_NB)
		auto const operation{LOCK_EX | (nonblocking ? LOCK_NB : 0)};
		for (;;)
		{
			if (::fast_io::noexcept_call(::flock, fd, operation) == 0)
			{
				return;
			}
			auto const error{errno};
			if (error == EINTR)
			{
				continue;
			}
			if (nonblocking && (error == EAGAIN || error == EWOULDBLOCK))
			{
				throw_posix_error(EADDRINUSE);
			}
			throw_posix_error(error);
		}
#elif defined(F_SETLK) && defined(F_SETLKW)
		struct ::flock descriptor_lock{};
		descriptor_lock.l_type = F_WRLCK;
		descriptor_lock.l_whence = SEEK_SET;
		auto const operation{nonblocking ? F_SETLK : F_SETLKW};
		for (;;)
		{
			if (::fcntl(fd, operation,
						__builtin_addressof(descriptor_lock)) == 0)
			{
				return;
			}
			auto const error{errno};
			if (error == EINTR)
			{
				continue;
			}
			if (nonblocking && (error == EACCES || error == EAGAIN))
			{
				throw_posix_error(EADDRINUSE);
			}
			throw_posix_error(error);
		}
#else
		(void)nonblocking;
		throw_posix_error(ENOTSUP);
#endif
	}
};

} // namespace fast_io::details
