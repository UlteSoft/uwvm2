#pragma once

#include "../mode.h"
#include "common.h"

#if __has_include(<sys/socket.h>)
#include <sys/socket.h>
#endif
#if __has_include(<sys/un.h>)
#include <sys/un.h>
#endif

namespace fast_io
{

namespace details
{

struct posix_named_pipe_address
{
	::sockaddr_un address{};
	::socklen_t length{};
	char8_t lock_path[128]{};
};

inline posix_named_pipe_address posix_named_pipe_make_address(
	char8_t const *name, ::std::size_t name_size)
{
	posix_ipc_validate_name(name, name_size);

	posix_named_pipe_address result{};
	result.address.sun_family = AF_UNIX;
	char8_t path[sizeof(result.address.sun_path)]{};
	constexpr ::std::uint64_t domain{0x706970655f6e616dULL};
	constexpr char8_t socket_suffix[]{u8".sock"};
	auto const iter{posix_ipc_make_user_path(
		path, path + sizeof(path), name, name_size, domain,
		socket_suffix, sizeof(socket_suffix) - 1u)};
	constexpr char8_t lock_suffix[]{u8".pipe.lock"};
	posix_ipc_make_user_path(
		result.lock_path, result.lock_path + sizeof(result.lock_path),
		name, name_size, domain, lock_suffix, sizeof(lock_suffix) - 1u);
	auto const path_size{static_cast<::std::size_t>(iter - path) + 1u};
	__builtin_memcpy(result.address.sun_path, path, path_size);
	auto address_size{offsetof(::sockaddr_un, sun_path) + path_size};
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
	defined(__OpenBSD__) || defined(__DragonFly__)
	--address_size;
#endif
	result.length = static_cast<::socklen_t>(
		address_size);
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
	defined(__OpenBSD__) || defined(__DragonFly__)
	result.address.sun_len = static_cast<decltype(result.address.sun_len)>(
		result.length);
#endif
	return result;
}

inline void posix_named_pipe_validate_mode(ipc_mode mode)
{
	constexpr auto supported_modes{
		ipc_mode::in | ipc_mode::out | ipc_mode::sync | ipc_mode::no_block |
		ipc_mode::message | ipc_mode::huge_page_2M | ipc_mode::huge_page_1G};
	if ((mode & ~supported_modes) != ipc_mode::none) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
	if ((mode & (ipc_mode::huge_page_2M | ipc_mode::huge_page_1G)) != ipc_mode::none) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
	if ((mode & (ipc_mode::in | ipc_mode::out)) == ipc_mode::none) [[unlikely]]
	{
		throw_posix_error(EINVAL);
	}
	if ((mode & ipc_mode::sync) != ipc_mode::none) [[unlikely]]
	{
		throw_posix_error(ENOTSUP);
	}
#if !defined(SOCK_SEQPACKET) || defined(__APPLE__)
	if ((mode & ipc_mode::message) != ipc_mode::none) [[unlikely]]
	{
		throw_posix_error(ENOTSUP);
	}
#endif
}

inline int posix_named_pipe_socket_type(ipc_mode mode) noexcept
{
	return (mode & ipc_mode::message) == ipc_mode::none
			   ? SOCK_STREAM
#if defined(SOCK_SEQPACKET)
			   : SOCK_SEQPACKET
#else
			   : SOCK_STREAM
#endif
		;
}

inline void posix_named_pipe_set_descriptor_flags(int fd, bool nonblocking)
{
	posix_ipc_set_close_on_exec(fd);
#if defined(F_GETFL) && defined(F_SETFL) && defined(O_NONBLOCK)
	if (nonblocking)
	{
		auto const status_flags{::fast_io::details::sys_fcntl(fd, F_GETFL)};
		if ((status_flags & O_NONBLOCK) == 0)
		{
			::fast_io::details::sys_fcntl(fd, F_SETFL, status_flags | O_NONBLOCK);
		}
	}
#else
	(void)nonblocking;
#endif
}

inline void posix_named_pipe_set_socket_options(int fd)
{
#ifdef SO_NOSIGPIPE
	int value{1};
	// POSIX specifies the option extent as socklen_t rather than size_t.  The
	// object is exactly one int, so this explicit conversion is representable
	// on every conforming socket ABI and prevents an implicit ABI-width change
	// at the noexcept_call forwarding boundary.
	constexpr ::socklen_t value_extent{static_cast<::socklen_t>(sizeof(value))};
	if (::fast_io::noexcept_call(::setsockopt, fd, SOL_SOCKET,
								 SO_NOSIGPIPE, __builtin_addressof(value), value_extent) == -1) [[unlikely]]
	{
		throw_posix_error();
	}
#else
	(void)fd;
#endif
}

inline int posix_named_pipe_create_socket(int socket_type, bool nonblocking)
{
	int flags{socket_type};
#ifdef SOCK_CLOEXEC
	flags |= SOCK_CLOEXEC;
#endif
#ifdef SOCK_NONBLOCK
	if (nonblocking)
	{
		flags |= SOCK_NONBLOCK;
	}
#endif
	int fd{::fast_io::noexcept_call(::socket, AF_UNIX, flags, 0)};
	if (fd == -1 && flags != socket_type && errno == EINVAL)
	{
		fd = ::fast_io::noexcept_call(::socket, AF_UNIX, socket_type, 0);
	}
	if (fd == -1) [[unlikely]]
	{
		throw_posix_error();
	}
	::fast_io::posix_file file{fd};
	posix_named_pipe_set_descriptor_flags(file.native_handle(), nonblocking);
	posix_named_pipe_set_socket_options(file.native_handle());
	return file.release();
}

inline void posix_named_pipe_apply_direction(int fd, ipc_mode mode)
{
	auto const access_mode{mode & (ipc_mode::in | ipc_mode::out)};
	int direction{-1};
	if (access_mode == ipc_mode::in)
	{
		direction = SHUT_WR;
	}
	else if (access_mode == ipc_mode::out)
	{
		direction = SHUT_RD;
	}
	if (direction != -1 && ::fast_io::noexcept_call(::shutdown, fd, direction) == -1) [[unlikely]]
	{
		throw_posix_error();
	}
}

struct posix_named_pipe_server_lifetime
{
	::std::atomic_size_t references{1u};
	posix_named_pipe_address socket_address{};
	int listener_fd{-1};
	int lock_fd{-1};
};

using posix_named_pipe_server_lifetime_allocator =
	::fast_io::native_typed_global_allocator<posix_named_pipe_server_lifetime>;

inline posix_named_pipe_server_lifetime *posix_named_pipe_allocate_lifetime(
	posix_named_pipe_address const &socket_address, int listener_fd, int lock_fd)
{
	auto ptr{posix_named_pipe_server_lifetime_allocator::allocate(1u)};
	::std::construct_at(ptr);
	ptr->socket_address = socket_address;
	ptr->listener_fd = listener_fd;
	ptr->lock_fd = lock_fd;
	return ptr;
}

inline posix_named_pipe_server_lifetime *posix_named_pipe_add_lifetime_reference(
	posix_named_pipe_server_lifetime *lifetime) noexcept
{
	if (lifetime != nullptr)
	{
		lifetime->references.fetch_add(1u, ::std::memory_order_relaxed);
	}
	return lifetime;
}

inline int posix_named_pipe_release_lifetime(
	posix_named_pipe_server_lifetime *lifetime) noexcept
{
	if (lifetime == nullptr ||
		lifetime->references.fetch_sub(1u, ::std::memory_order_acq_rel) != 1u)
	{
		return 0;
	}
	int error{};
	auto const listener_close_error{
		posix_ipc_close_nothrow(lifetime->listener_fd)};
	if (listener_close_error != 0)
	{
		error = listener_close_error;
	}
	if (::fast_io::noexcept_call(::unlink,
								 lifetime->socket_address.address.sun_path) == -1 &&
		errno != ENOENT)
	{
		if (error == 0)
		{
			error = errno;
		}
	}
	auto const close_error{posix_ipc_close_nothrow(lifetime->lock_fd)};
	if (error == 0)
	{
		error = close_error;
	}
	::std::destroy_at(lifetime);
	posix_named_pipe_server_lifetime_allocator::deallocate_n(lifetime, 1u);
	return error;
}

inline void posix_named_pipe_remove_stale_path(
	posix_named_pipe_address const &socket_address, bool require_existing = false)
{
	struct ::stat status{};
	if (::fast_io::noexcept_call(::lstat, socket_address.address.sun_path,
								 __builtin_addressof(status)) == -1)
	{
		if (errno == ENOENT)
		{
			if (require_existing) [[unlikely]]
			{
				throw_posix_error(ENOENT);
			}
			return;
		}
		throw_posix_error();
	}
	if (!S_ISSOCK(status.st_mode)) [[unlikely]]
	{
		throw_posix_error(EADDRINUSE);
	}
	if (status.st_uid != ::geteuid()) [[unlikely]]
	{
		throw_posix_error(EACCES);
	}
	if (::fast_io::noexcept_call(::unlink,
								 socket_address.address.sun_path) == -1) [[unlikely]]
	{
		throw_posix_error();
	}
}

struct posix_named_pipe_path_guard
{
	posix_named_pipe_address const *socket_address{};
	bool armed{};

	inline ~posix_named_pipe_path_guard()
	{
		if (armed)
		{
			::fast_io::noexcept_call(::unlink,
									 socket_address->address.sun_path);
		}
	}
};

struct posix_named_pipe_handle_state
{
	int fd{-1};
	posix_named_pipe_server_lifetime *lifetime{};
};

inline posix_named_pipe_handle_state posix_create_named_pipe_ipc_server_impl(
	char8_t const *name, ::std::size_t name_size, ipc_mode mode)
{
	posix_named_pipe_validate_mode(mode);
	auto const socket_address{posix_named_pipe_make_address(name, name_size)};
	posix_ipc_file_lock pathname_lock{socket_address.lock_path, true};
	posix_named_pipe_remove_stale_path(socket_address);
	auto const socket_type{posix_named_pipe_socket_type(mode)};
	::fast_io::posix_file listener{
		posix_named_pipe_create_socket(socket_type, (mode & ipc_mode::no_block) != ipc_mode::none)};
#if defined(__FreeBSD__)
	if (::fast_io::noexcept_call(::fchmod, listener.native_handle(),
								 static_cast<::mode_t>(S_IRUSR | S_IWUSR)) == -1) [[unlikely]]
	{
		throw_posix_error();
	}
#endif
	auto bind_result{::fast_io::noexcept_call(
		::bind, listener.native_handle(), reinterpret_cast<::sockaddr const *>(__builtin_addressof(socket_address.address)), socket_address.length)};
	if (bind_result == -1) [[unlikely]]
	{
		throw_posix_error();
	}
	posix_named_pipe_path_guard path_guard{
		__builtin_addressof(socket_address), true};
	if (::fast_io::noexcept_call(::chmod, socket_address.address.sun_path,
								 static_cast<::mode_t>(S_IRUSR | S_IWUSR)) == -1) [[unlikely]]
	{
		throw_posix_error();
	}
	if (::fast_io::noexcept_call(::listen, listener.native_handle(), SOMAXCONN) == -1) [[unlikely]]
	{
		throw_posix_error();
	}
	::fast_io::posix_file server_descriptor{
		posix_ipc_duplicate_close_on_exec(listener.native_handle())};
	auto const lifetime{posix_named_pipe_allocate_lifetime(
		socket_address, listener.native_handle(), pathname_lock.fd)};
	listener.release();
	pathname_lock.fd = -1;
	path_guard.armed = false;
	return {server_descriptor.release(), lifetime};
}

struct posix_create_named_pipe_ipc_server_parameter
{
	ipc_mode mode{};
	inline posix_named_pipe_handle_state operator()(
		char8_t const *name, ::std::size_t name_size) const
	{
		return posix_create_named_pipe_ipc_server_impl(name, name_size, mode);
	}
};

template <typename T>
	requires(::fast_io::constructible_to_os_c_str<T>)
inline posix_named_pipe_handle_state posix_create_named_pipe_ipc_server_impl(
	T const &name, ipc_mode mode)
{
	return ::fast_io::details::posix_ipc_u8_api_common(
		name, posix_create_named_pipe_ipc_server_parameter{mode});
}

inline int posix_create_named_pipe_ipc_client_impl(
	char8_t const *name, ::std::size_t name_size, ipc_mode mode)
{
	posix_named_pipe_validate_mode(mode);
	auto const socket_address{posix_named_pipe_make_address(name, name_size)};
	::fast_io::posix_file connection{posix_named_pipe_create_socket(
		posix_named_pipe_socket_type(mode), (mode & ipc_mode::no_block) != ipc_mode::none)};
	auto const result{::fast_io::noexcept_call(
		::connect, connection.native_handle(), reinterpret_cast<::sockaddr const *>(__builtin_addressof(socket_address.address)), socket_address.length)};
	if (result == -1) [[unlikely]]
	{
		throw_posix_error();
	}
	posix_named_pipe_apply_direction(connection.native_handle(), mode);
	return connection.release();
}

struct posix_create_named_pipe_ipc_client_parameter
{
	ipc_mode mode{};
	inline int operator()(char8_t const *name, ::std::size_t name_size) const
	{
		return posix_create_named_pipe_ipc_client_impl(name, name_size, mode);
	}
};

template <typename T>
	requires(::fast_io::constructible_to_os_c_str<T>)
inline int posix_create_named_pipe_ipc_client_impl(T const &name, ipc_mode mode)
{
	return ::fast_io::details::posix_ipc_u8_api_common(
		name, posix_create_named_pipe_ipc_client_parameter{mode});
}

inline int posix_named_pipe_accept_impl(int listener_fd, ipc_mode mode)
{
	bool const nonblocking{(mode & ipc_mode::no_block) != ipc_mode::none};
	int accepted_fd{-1};
#if defined(__linux__) && defined(__NR_accept4)
	int flags{};
#ifdef SOCK_CLOEXEC
	flags |= SOCK_CLOEXEC;
#endif
#ifdef SOCK_NONBLOCK
	if (nonblocking)
	{
		flags |= SOCK_NONBLOCK;
	}
#endif
	accepted_fd = ::fast_io::system_call<__NR_accept4, int>(
		listener_fd, nullptr, nullptr, flags);
	if (::fast_io::linux_system_call_fails(accepted_fd))
	{
		if (-accepted_fd != ENOSYS) [[unlikely]]
		{
			::fast_io::system_call_throw_error(accepted_fd);
		}
		accepted_fd = ::fast_io::noexcept_call(
			::accept, listener_fd, nullptr, nullptr);
		if (accepted_fd == -1) [[unlikely]]
		{
			throw_posix_error();
		}
	}
#elif defined(__FreeBSD__)
	int flags{};
#ifdef SOCK_CLOEXEC
	flags |= SOCK_CLOEXEC;
#endif
#ifdef SOCK_NONBLOCK
	if (nonblocking)
	{
		flags |= SOCK_NONBLOCK;
	}
#endif
	accepted_fd = ::fast_io::noexcept_call(
		::accept4, listener_fd, nullptr, nullptr, flags);
	if (accepted_fd == -1 && errno == ENOSYS)
	{
		accepted_fd = ::fast_io::noexcept_call(
			::accept, listener_fd, nullptr, nullptr);
	}
	if (accepted_fd == -1) [[unlikely]]
	{
		throw_posix_error();
	}
#else
	accepted_fd = ::fast_io::noexcept_call(::accept, listener_fd, nullptr, nullptr);
	if (accepted_fd == -1) [[unlikely]]
	{
		throw_posix_error();
	}
#endif
	::fast_io::posix_file accepted{accepted_fd};
	posix_named_pipe_set_descriptor_flags(accepted.native_handle(), nonblocking);
	posix_named_pipe_set_socket_options(accepted.native_handle());
	posix_named_pipe_apply_direction(accepted.native_handle(), mode);
	return accepted.release();
}

inline void posix_unlink_named_pipe_ipc_impl(
	char8_t const *name, ::std::size_t name_size)
{
	auto const socket_address{posix_named_pipe_make_address(name, name_size)};
	posix_ipc_file_lock pathname_lock{socket_address.lock_path, true};
	posix_named_pipe_remove_stale_path(socket_address, true);
}

} // namespace details

template <::std::integral ch_type>
class basic_posix_named_pipe_ipc_observer
{
public:
	using char_type = ch_type;
	using input_char_type = char_type;
	using output_char_type = char_type;
	using native_handle_type = int;

	native_handle_type fd{-1};

	inline constexpr native_handle_type native_handle() const noexcept
	{
		return fd;
	}

	inline explicit constexpr operator bool() const noexcept
	{
		return fd != -1;
	}

	inline explicit constexpr operator basic_posix_io_observer<ch_type>() const noexcept
	{
		return {fd};
	}

	inline constexpr native_handle_type release() noexcept
	{
		auto const result{fd};
		fd = -1;
		return result;
	}
};

template <::std::integral ch_type>
using basic_posix_named_pipe_ipc_server_observer =
	basic_posix_named_pipe_ipc_observer<ch_type>;

template <::std::integral ch_type>
using basic_posix_named_pipe_ipc_client_observer =
	basic_posix_named_pipe_ipc_observer<ch_type>;

template <::std::integral ch_type>
inline constexpr basic_posix_named_pipe_ipc_observer<ch_type>
io_stream_ref_define(basic_posix_named_pipe_ipc_observer<ch_type> observer) noexcept
{
	return observer;
}

template <::std::integral ch_type>
inline constexpr basic_posix_named_pipe_ipc_observer<char>
io_bytes_stream_ref_define(basic_posix_named_pipe_ipc_observer<ch_type> observer) noexcept
{
	return {observer.fd};
}

template <::std::integral ch_type>
inline ::std::byte *read_some_bytes_underflow_define(
	basic_posix_named_pipe_ipc_observer<ch_type> observer,
	::std::byte *first, ::std::byte *last)
{
	return ::fast_io::details::posix_read_bytes_impl(observer.fd, first, last);
}

template <::std::integral ch_type>
inline ::std::byte const *write_some_bytes_overflow_define(
	basic_posix_named_pipe_ipc_observer<ch_type> observer,
	::std::byte const *first, ::std::byte const *last)
{
	int flags{};
#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif
	auto const result{::fast_io::noexcept_call(::send, observer.fd, first,
											   static_cast<::std::size_t>(last - first), flags)};
	if (result == -1) [[unlikely]]
	{
		throw_posix_error();
	}
	return first + result;
}

template <::std::integral ch_type>
inline ::fast_io::io_scatter_status_t scatter_read_some_bytes_underflow_define(
	basic_posix_named_pipe_ipc_observer<ch_type> observer,
	::fast_io::io_scatter_t const *scatters, ::std::size_t count)
{
	return ::fast_io::details::posix_scatter_read_bytes_impl(
		observer.fd, scatters, count);
}

template <::std::integral ch_type>
inline ::fast_io::io_scatter_status_t posix_named_pipe_scatter_write(
	basic_posix_named_pipe_ipc_observer<ch_type> observer,
	::fast_io::io_scatter_t const *scatters, ::std::size_t count)
{
	count = ::std::min(count,
					   ::fast_io::details::posix_scatter_maximum_count);
	using iovec_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= struct ::iovec const *;
	::msghdr message{};
	message.msg_iov = const_cast<struct ::iovec *>(
		reinterpret_cast<iovec_may_alias_const_ptr>(scatters));
	message.msg_iovlen = static_cast<decltype(message.msg_iovlen)>(count);
	int flags{};
#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif
	auto const result{::fast_io::noexcept_call(
		::sendmsg, observer.fd, __builtin_addressof(message), flags)};
	if (result == -1) [[unlikely]]
	{
		throw_posix_error();
	}
	return ::fast_io::scatter_size_to_status(
		static_cast<::std::size_t>(result), scatters, count);
}

template <::std::integral ch_type>
inline ::fast_io::io_scatter_status_t scatter_write_some_bytes_overflow_define(
	basic_posix_named_pipe_ipc_observer<ch_type> observer,
	::fast_io::io_scatter_t const *scatters, ::std::size_t count)
{
	return posix_named_pipe_scatter_write(observer, scatters, count);
}

template <::std::integral ch_type>
inline ::fast_io::io_scatter_status_t print_static_scatter_write_some_bytes_overflow_define(
	basic_posix_named_pipe_ipc_observer<ch_type> observer,
	::fast_io::io_scatter_t const *scatters, ::std::size_t count)
{
	return posix_named_pipe_scatter_write(observer, scatters, count);
}

template <::std::integral ch_type, bool server>
class basic_posix_named_pipe_ipc_handle : public basic_posix_named_pipe_ipc_observer<ch_type>
{
public:
	using observer_type = basic_posix_named_pipe_ipc_observer<ch_type>;
	using typename observer_type::char_type;
	using typename observer_type::input_char_type;
	using typename observer_type::output_char_type;
	using typename observer_type::native_handle_type;
	using observer_type::native_handle;

	inline constexpr basic_posix_named_pipe_ipc_handle() noexcept = default;

	inline constexpr basic_posix_named_pipe_ipc_handle(
		observer_type) noexcept = delete;
	inline constexpr basic_posix_named_pipe_ipc_handle &operator=(
		observer_type) noexcept = delete;

	inline basic_posix_named_pipe_ipc_handle(
		basic_posix_named_pipe_ipc_handle const &other)
		: observer_type{other.fd == -1 ? -1 : ::fast_io::details::posix_ipc_duplicate_close_on_exec(other.fd)},
		  connection_mode{other.connection_mode},
		  server_connected{other.server_connected},
		  server_lifetime{::fast_io::details::posix_named_pipe_add_lifetime_reference(
			  other.server_lifetime)}
	{}

	inline basic_posix_named_pipe_ipc_handle &operator=(
		basic_posix_named_pipe_ipc_handle const &other)
	{
		if (__builtin_addressof(other) == this) [[unlikely]]
		{
			return *this;
		}
		auto const new_fd{other.fd == -1 ? -1 : ::fast_io::details::posix_ipc_duplicate_close_on_exec(other.fd)};
		auto const new_lifetime{
			::fast_io::details::posix_named_pipe_add_lifetime_reference(
				other.server_lifetime)};
		auto const old_fd{this->fd};
		auto const old_lifetime{server_lifetime};
		this->fd = new_fd;
		server_lifetime = new_lifetime;
		connection_mode = other.connection_mode;
		server_connected = other.server_connected;
		::fast_io::details::posix_ipc_close_nothrow(old_fd);
		::fast_io::details::posix_named_pipe_release_lifetime(old_lifetime);
		return *this;
	}

	inline basic_posix_named_pipe_ipc_handle(
		basic_posix_named_pipe_ipc_handle &&other) noexcept
		: observer_type{other.fd}, connection_mode{other.connection_mode},
		  server_connected{other.server_connected},
		  server_lifetime{other.server_lifetime}
	{
		other.fd = -1;
		other.server_connected = false;
		other.server_lifetime = nullptr;
	}

	inline basic_posix_named_pipe_ipc_handle &operator=(
		basic_posix_named_pipe_ipc_handle &&other) noexcept
	{
		if (__builtin_addressof(other) != this) [[likely]]
		{
			reset();
			this->fd = other.fd;
			connection_mode = other.connection_mode;
			server_connected = other.server_connected;
			server_lifetime = other.server_lifetime;
			other.fd = -1;
			other.server_connected = false;
			other.server_lifetime = nullptr;
		}
		return *this;
	}

	inline void reset(native_handle_type new_fd = -1) noexcept
	{
		auto const old_fd{this->fd};
		auto const old_lifetime{server_lifetime};
		this->fd = new_fd;
		server_connected = false;
		server_lifetime = nullptr;
		::fast_io::details::posix_ipc_close_nothrow(old_fd);
		::fast_io::details::posix_named_pipe_release_lifetime(old_lifetime);
	}

	inline void close()
	{
		auto const old_fd{this->fd};
		auto const old_lifetime{server_lifetime};
		this->fd = -1;
		server_connected = false;
		server_lifetime = nullptr;
		auto error{::fast_io::details::posix_ipc_close_nothrow(old_fd)};
		auto const lifetime_error{
			::fast_io::details::posix_named_pipe_release_lifetime(old_lifetime)};
		if (error == 0)
		{
			error = lifetime_error;
		}
		if (error != 0) [[unlikely]]
		{
			throw_posix_error(error);
		}
	}

	template <typename native_hd>
		requires ::std::same_as<native_handle_type, ::std::remove_cvref_t<native_hd>>
	inline explicit constexpr basic_posix_named_pipe_ipc_handle(
		native_hd handle, ipc_mode requested_mode = ipc_mode::in | ipc_mode::out) noexcept
		: observer_type{handle}, connection_mode{requested_mode}
	{}

	inline basic_posix_named_pipe_ipc_handle(
		io_dup_t, observer_type observer,
		ipc_mode requested_mode = ipc_mode::in | ipc_mode::out)
		: observer_type{::fast_io::details::posix_ipc_duplicate_close_on_exec(observer.fd)},
		  connection_mode{requested_mode}
	{}

	template <::fast_io::constructible_to_os_c_str T>
	inline explicit basic_posix_named_pipe_ipc_handle(
		T const &name, ipc_mode requested_mode)
		: connection_mode{requested_mode}
	{
		if constexpr (server)
		{
			auto const state{
				::fast_io::details::posix_create_named_pipe_ipc_server_impl(
					name, requested_mode)};
			this->fd = state.fd;
			server_lifetime = state.lifetime;
		}
		else
		{
			this->fd = ::fast_io::details::posix_create_named_pipe_ipc_client_impl(
				name, requested_mode);
		}
	}

	inline constexpr native_handle_type release() noexcept
		requires(!server)
	{
		return observer_type::release();
	}

	inline native_handle_type release() noexcept
		requires(server)
	= delete;

	inline native_handle_type listener_native_handle() const noexcept
		requires(server)
	{
		return server_lifetime == nullptr ? this->fd : server_lifetime->listener_fd;
	}

	inline bool has_managed_listener() const noexcept
		requires(server)
	{
		return server_lifetime != nullptr;
	}

	inline void install_server_connection(native_handle_type connected_fd) noexcept
		requires(server)
	{
		auto const old_fd{this->fd};
		this->fd = connected_fd;
		server_connected = true;
		::fast_io::details::posix_ipc_close_nothrow(old_fd);
	}

	inline void disconnect_server()
		requires(server)
	{
		if (!server_connected || server_lifetime == nullptr)
		{
			return;
		}
		auto const listener_duplicate{
			::fast_io::details::posix_ipc_duplicate_close_on_exec(
				server_lifetime->listener_fd)};
		auto const old_fd{this->fd};
		this->fd = listener_duplicate;
		server_connected = false;
		::fast_io::details::posix_ipc_close_nothrow(old_fd);
	}

	inline ~basic_posix_named_pipe_ipc_handle()
	{
		reset();
	}

	ipc_mode connection_mode{};

private:
	bool server_connected{};
	::fast_io::details::posix_named_pipe_server_lifetime *server_lifetime{};
};

template <::std::integral ch_type>
using basic_posix_named_pipe_ipc_server =
	basic_posix_named_pipe_ipc_handle<ch_type, true>;

template <::std::integral ch_type>
using basic_posix_named_pipe_ipc_client =
	basic_posix_named_pipe_ipc_handle<ch_type, false>;

template <::std::integral server_ch_type, ::std::integral client_ch_type = char>
inline basic_posix_named_pipe_ipc_client<client_ch_type> wait_for_connect(
	basic_posix_named_pipe_ipc_server<server_ch_type> &server)
{
	auto const accepted_fd{::fast_io::details::posix_named_pipe_accept_impl(
		server.listener_native_handle(), server.connection_mode)};
	if (server.has_managed_listener())
	{
		server.install_server_connection(accepted_fd);
		return {};
	}
	return basic_posix_named_pipe_ipc_client<client_ch_type>{
		accepted_fd, server.connection_mode};
}

template <::std::integral server_ch_type, ::std::integral client_ch_type = char>
inline basic_posix_named_pipe_ipc_client<client_ch_type> wait_for_connect(
	basic_posix_named_pipe_ipc_server_observer<server_ch_type> server)
{
	constexpr auto default_mode{ipc_mode::in | ipc_mode::out};
	return basic_posix_named_pipe_ipc_client<client_ch_type>{
		::fast_io::details::posix_named_pipe_accept_impl(server.fd, default_mode),
		default_mode};
}

template <::std::integral server_ch_type, ::std::integral client_ch_type>
inline void accept_connect(
	basic_posix_named_pipe_ipc_server_observer<server_ch_type>,
	basic_posix_named_pipe_ipc_client_observer<client_ch_type>, bool) noexcept
{}

template <::std::integral server_ch_type>
inline void disconnect(
	basic_posix_named_pipe_ipc_server<server_ch_type> &server)
{
	server.disconnect_server();
}

template <::std::integral server_ch_type>
inline void disconnect(
	basic_posix_named_pipe_ipc_server_observer<server_ch_type>) noexcept
{}

template <::fast_io::constructible_to_os_c_str T>
inline void posix_unlink_named_pipe_ipc(T const &name)
{
	::fast_io::details::posix_ipc_u8_api_common(name, [](char8_t const *native_name, ::std::size_t native_name_size) {
		::fast_io::details::posix_unlink_named_pipe_ipc_impl(native_name, native_name_size);
	});
}

using posix_named_pipe_ipc_server_observer = basic_posix_named_pipe_ipc_server_observer<char>;
using wposix_named_pipe_ipc_server_observer = basic_posix_named_pipe_ipc_server_observer<wchar_t>;
using u8posix_named_pipe_ipc_server_observer = basic_posix_named_pipe_ipc_server_observer<char8_t>;
using u16posix_named_pipe_ipc_server_observer = basic_posix_named_pipe_ipc_server_observer<char16_t>;
using u32posix_named_pipe_ipc_server_observer = basic_posix_named_pipe_ipc_server_observer<char32_t>;

using posix_named_pipe_ipc_server = basic_posix_named_pipe_ipc_server<char>;
using wposix_named_pipe_ipc_server = basic_posix_named_pipe_ipc_server<wchar_t>;
using u8posix_named_pipe_ipc_server = basic_posix_named_pipe_ipc_server<char8_t>;
using u16posix_named_pipe_ipc_server = basic_posix_named_pipe_ipc_server<char16_t>;
using u32posix_named_pipe_ipc_server = basic_posix_named_pipe_ipc_server<char32_t>;

using posix_named_pipe_ipc_client_observer = basic_posix_named_pipe_ipc_client_observer<char>;
using wposix_named_pipe_ipc_client_observer = basic_posix_named_pipe_ipc_client_observer<wchar_t>;
using u8posix_named_pipe_ipc_client_observer = basic_posix_named_pipe_ipc_client_observer<char8_t>;
using u16posix_named_pipe_ipc_client_observer = basic_posix_named_pipe_ipc_client_observer<char16_t>;
using u32posix_named_pipe_ipc_client_observer = basic_posix_named_pipe_ipc_client_observer<char32_t>;

using posix_named_pipe_ipc_client = basic_posix_named_pipe_ipc_client<char>;
using wposix_named_pipe_ipc_client = basic_posix_named_pipe_ipc_client<wchar_t>;
using u8posix_named_pipe_ipc_client = basic_posix_named_pipe_ipc_client<char8_t>;
using u16posix_named_pipe_ipc_client = basic_posix_named_pipe_ipc_client<char16_t>;
using u32posix_named_pipe_ipc_client = basic_posix_named_pipe_ipc_client<char32_t>;

namespace freestanding
{

template <::std::integral char_type, bool server>
struct is_trivially_copyable_or_relocatable<
	::fast_io::basic_posix_named_pipe_ipc_handle<char_type, server>>
{
	inline static constexpr bool value = true;
};

} // namespace freestanding

} // namespace fast_io
