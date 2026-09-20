#pragma once

#include "../mode.h"
#include "common.h"

#if __has_include(<sys/eventfd.h>)
#include <sys/eventfd.h>
#endif

namespace fast_io
{

#if defined(__linux__) || __has_include(<sys/eventfd.h>)

namespace details
{

inline void posix_eventfd_validate_mode(ipc_mode mode)
{
	constexpr auto supported_modes{
		ipc_mode::in | ipc_mode::out | ipc_mode::no_block | ipc_mode::message};
	if ((mode & ~supported_modes) != ipc_mode::none) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
}

inline int posix_eventfd_create_impl(::std::uint64_t initial_value, ipc_mode mode)
{
	posix_eventfd_validate_mode(mode);
	if (initial_value > ::std::numeric_limits<unsigned>::max()) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
	int flags{};
#ifdef EFD_CLOEXEC
	flags |= EFD_CLOEXEC;
#endif
#ifdef EFD_NONBLOCK
	if ((mode & ipc_mode::no_block) != ipc_mode::none)
	{
		flags |= EFD_NONBLOCK;
	}
#endif
#ifdef EFD_SEMAPHORE
	if ((mode & ipc_mode::message) != ipc_mode::none)
	{
		flags |= EFD_SEMAPHORE;
	}
#else
	if ((mode & ipc_mode::message) != ipc_mode::none) [[unlikely]]
	{
		throw_posix_error(ENOTSUP);
	}
#endif
	int fd{-1};
#if defined(__linux__) && defined(__NR_eventfd2)
	fd = ::fast_io::system_call<__NR_eventfd2, int>(
		static_cast<unsigned>(initial_value), flags);
	if (::fast_io::linux_system_call_fails(fd))
	{
		if (-fd != ENOSYS) [[unlikely]]
		{
			::fast_io::system_call_throw_error(fd);
		}
#if defined(__NR_eventfd)
		if ((mode & ipc_mode::message) != ipc_mode::none) [[unlikely]]
		{
			throw_posix_error(ENOTSUP);
		}
		fd = ::fast_io::system_call<__NR_eventfd, int>(
			static_cast<unsigned>(initial_value));
		::fast_io::system_call_throw_error(fd);
#else
		throw_posix_error(ENOSYS);
#endif
	}
#elif defined(__linux__) && defined(__NR_eventfd)
	if ((mode & ipc_mode::message) != ipc_mode::none) [[unlikely]]
	{
		throw_posix_error(ENOTSUP);
	}
	fd = ::fast_io::system_call<__NR_eventfd, int>(
		static_cast<unsigned>(initial_value));
	::fast_io::system_call_throw_error(fd);
#else
	fd = ::fast_io::noexcept_call(
		::eventfd, static_cast<unsigned>(initial_value), flags);
	if (fd == -1) [[unlikely]]
	{
		throw_posix_error();
	}
#endif
	::fast_io::posix_file file{fd};
	{
		posix_ipc_set_close_on_exec(file.native_handle());
#if defined(F_GETFL) && defined(F_SETFL) && defined(O_NONBLOCK)
		if ((mode & ipc_mode::no_block) != ipc_mode::none)
		{
			auto const status_flags{::fast_io::details::sys_fcntl(fd, F_GETFL)};
			::fast_io::details::sys_fcntl(fd, F_SETFL, status_flags | O_NONBLOCK);
		}
#endif
	}
	return file.release();
}

inline ::std::uint64_t posix_eventfd_read_impl(int fd)
{
	::std::uint64_t value{};
	static_assert(sizeof(value) == 8u);
	auto const first{reinterpret_cast<::std::byte *>(__builtin_addressof(value))};
	auto const last{first + sizeof(value)};
	if (::fast_io::details::posix_read_bytes_impl(fd, first, last) != last) [[unlikely]]
	{
		throw_posix_error(EIO);
	}
	return value;
}

inline void posix_eventfd_write_impl(int fd, ::std::uint64_t value)
{
	static_assert(sizeof(value) == 8u);
	if (value == ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
	auto const first{reinterpret_cast<::std::byte const *>(__builtin_addressof(value))};
	auto const last{first + sizeof(value)};
	if (::fast_io::details::posix_write_bytes_impl(fd, first, last) != last) [[unlikely]]
	{
		throw_posix_error(EIO);
	}
}

} // namespace details

template <::std::integral ch_type>
using basic_posix_eventfd_observer = basic_posix_io_observer<ch_type>;

template <::std::integral ch_type>
class basic_posix_eventfd : public basic_posix_eventfd_observer<ch_type>
{
public:
	using typename basic_posix_eventfd_observer<ch_type>::char_type;
	using typename basic_posix_eventfd_observer<ch_type>::input_char_type;
	using typename basic_posix_eventfd_observer<ch_type>::output_char_type;
	using typename basic_posix_eventfd_observer<ch_type>::native_handle_type;
	using basic_posix_eventfd_observer<ch_type>::native_handle;

	inline constexpr basic_posix_eventfd() noexcept = default;

	inline constexpr basic_posix_eventfd(
		basic_posix_eventfd_observer<ch_type>) noexcept = delete;
	inline constexpr basic_posix_eventfd &operator=(
		basic_posix_eventfd_observer<ch_type>) noexcept = delete;

	inline explicit basic_posix_eventfd(
		::std::uint64_t initial_value, ipc_mode mode = ipc_mode::none)
		: basic_posix_eventfd_observer<ch_type>{
			  ::fast_io::details::posix_eventfd_create_impl(initial_value, mode)}
	{}

	template <typename native_hd>
		requires ::std::same_as<native_handle_type, ::std::remove_cvref_t<native_hd>>
	inline explicit constexpr basic_posix_eventfd(native_hd handle) noexcept
		: basic_posix_eventfd_observer<ch_type>{handle}
	{}

	inline basic_posix_eventfd(
		io_dup_t, basic_posix_eventfd_observer<ch_type> observer)
		: basic_posix_eventfd_observer<ch_type>{
			  ::fast_io::details::posix_ipc_duplicate_close_on_exec(observer.fd)}
	{}

	inline basic_posix_eventfd(basic_posix_eventfd const &other)
		: basic_posix_eventfd_observer<ch_type>{
			  other.fd == -1 ? -1 : ::fast_io::details::posix_ipc_duplicate_close_on_exec(other.fd)}
	{}

	inline basic_posix_eventfd &operator=(basic_posix_eventfd const &other)
	{
		if (__builtin_addressof(other) == this) [[unlikely]]
		{
			return *this;
		}
		auto const new_fd{other.fd == -1 ? -1 : ::fast_io::details::posix_ipc_duplicate_close_on_exec(other.fd)};
		auto const old_fd{this->fd};
		this->fd = new_fd;
		::fast_io::details::posix_ipc_close_nothrow(old_fd);
		return *this;
	}

	inline basic_posix_eventfd(basic_posix_eventfd &&other) noexcept
		: basic_posix_eventfd_observer<ch_type>{other.release()}
	{}

	inline basic_posix_eventfd &operator=(basic_posix_eventfd &&other) noexcept
	{
		if (__builtin_addressof(other) != this) [[likely]]
		{
			reset();
			this->fd = other.release();
		}
		return *this;
	}

	inline void reset(native_handle_type new_fd = -1) noexcept
	{
		auto const old_fd{this->fd};
		this->fd = new_fd;
		::fast_io::details::posix_ipc_close_nothrow(old_fd);
	}

	inline void close()
	{
		if (this->fd != -1) [[likely]]
		{
			::fast_io::details::sys_close_throw_error(this->fd);
		}
	}

	inline ~basic_posix_eventfd()
	{
		reset();
	}
};

template <::std::integral ch_type>
inline ::std::uint64_t eventfd_read(
	basic_posix_eventfd_observer<ch_type> observer)
{
	return ::fast_io::details::posix_eventfd_read_impl(observer.fd);
}

template <::std::integral ch_type>
inline void eventfd_write(
	basic_posix_eventfd_observer<ch_type> observer, ::std::uint64_t value)
{
	::fast_io::details::posix_eventfd_write_impl(observer.fd, value);
}

using posix_eventfd_observer = basic_posix_eventfd_observer<char>;
using posix_eventfd = basic_posix_eventfd<char>;

namespace freestanding
{

template <::std::integral char_type>
struct is_trivially_copyable_or_relocatable<
	::fast_io::basic_posix_eventfd<char_type>>
{
	inline static constexpr bool value = true;
};

} // namespace freestanding

#endif

} // namespace fast_io
