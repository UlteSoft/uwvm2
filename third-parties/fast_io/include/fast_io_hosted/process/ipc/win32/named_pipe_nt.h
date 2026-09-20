#pragma once

#include "../mode.h"

namespace fast_io
{

namespace win32::nt::details
{

using nt_named_pipe_char_type = char16_t;
using nt_named_pipe_internal_str =
	::fast_io::containers::basic_string<nt_named_pipe_char_type, ::fast_io::native_global_allocator>;
using nt_named_pipe_internal_tlc_str =
	::fast_io::containers::basic_string<nt_named_pipe_char_type, ::fast_io::native_thread_local_allocator>;
using nt_named_pipe_internal_strvw = ::fast_io::containers::basic_string_view<nt_named_pipe_char_type>;

template <typename... Args>
constexpr inline nt_named_pipe_internal_str concat_nt_named_pipe_internal_str(Args &&...args)
{
	return ::fast_io::basic_general_concat_checked<false, nt_named_pipe_char_type,
												   nt_named_pipe_internal_str>(args...);
}

template <typename... Args>
constexpr inline nt_named_pipe_internal_tlc_str concat_nt_named_pipe_internal_tlc_str(Args &&...args)
{
	return ::fast_io::basic_general_concat_checked<false, nt_named_pipe_char_type,
												   nt_named_pipe_internal_tlc_str>(args...);
}

inline nt_named_pipe_internal_tlc_str
nt_named_pipe_path(nt_named_pipe_char_type const *server_name, ::std::size_t server_name_size)
{
	if (::fast_io::details::is_invalid_dos_filename_with_size(server_name, server_name_size)) [[unlikely]]
	{
		throw_nt_error(0xC000003Au); // STATUS_OBJECT_PATH_NOT_FOUND
	}

	auto pipe_name{concat_nt_named_pipe_internal_tlc_str(
		u"\\Device\\NamedPipe\\fast_io_ipc\\",
		::fast_io::mnp::os_c_str_with_known_size(server_name, server_name_size))};
	for (auto curr{pipe_name.data()}, last{curr + pipe_name.size()}; curr != last; ++curr)
	{
		if (*curr == u'/')
		{
			*curr = u'\\';
		}
	}
	return pipe_name;
}

template <nt_family family>
	requires(family == nt_family::nt || family == nt_family::zw)
inline void *nt_family_create_named_pipe_ipc_server_impl(
	nt_named_pipe_char_type const *server_name, ::std::size_t server_name_size, ::fast_io::ipc_mode mode)
{
	auto pipe_name{nt_named_pipe_path(server_name, server_name_size)};
	auto const pipe_name_bytes{
		::fast_io::win32::nt::details::strlen_to_nt_filename_bytes(pipe_name.size())};
	::fast_io::win32::nt::unicode_string pipe_name_unicode{
		.Length = pipe_name_bytes,
		.MaximumLength = pipe_name_bytes,
		.Buffer = pipe_name.data()};
	::fast_io::win32::nt::object_attributes object_attributes{
		.Length = sizeof(::fast_io::win32::nt::object_attributes),
		.RootDirectory = nullptr,
		.ObjectName = __builtin_addressof(pipe_name_unicode),
		.Attributes = 0x00000040u, // OBJ_CASE_INSENSITIVE
		.SecurityDescriptor = nullptr,
		.SecurityQualityOfService = nullptr};

	::std::uint_least32_t desired_access{0x00100000u}; // SYNCHRONIZE
	::std::uint_least32_t share_access{};
	if ((mode & ::fast_io::ipc_mode::in) == ::fast_io::ipc_mode::in)
	{
		desired_access |= 0x80000000u; // GENERIC_READ
		share_access |= 0x00000002u;   // FILE_SHARE_WRITE
	}
	if ((mode & ::fast_io::ipc_mode::out) == ::fast_io::ipc_mode::out)
	{
		desired_access |= 0x40000000u; // GENERIC_WRITE
		share_access |= 0x00000001u;   // FILE_SHARE_READ
	}

	::std::uint_least32_t create_options{};
	if ((mode & ::fast_io::ipc_mode::sync) != ::fast_io::ipc_mode::none)
	{
		create_options |= 0x00000002u; // FILE_WRITE_THROUGH
	}
	if ((mode & ::fast_io::ipc_mode::no_block) == ::fast_io::ipc_mode::none)
	{
		create_options |= 0x00000020u; // FILE_SYNCHRONOUS_IO_NONALERT
	}

	::std::uint_least32_t const pipe_type{
		(mode & ::fast_io::ipc_mode::message) == ::fast_io::ipc_mode::none
			? 0x00000000u   // FILE_PIPE_BYTE_STREAM_TYPE
			: 0x00000001u}; // FILE_PIPE_MESSAGE_TYPE
	::std::uint_least32_t const read_mode{
		(mode & ::fast_io::ipc_mode::message) == ::fast_io::ipc_mode::none
			? 0x00000000u   // FILE_PIPE_BYTE_STREAM_MODE
			: 0x00000001u}; // FILE_PIPE_MESSAGE_MODE

	::std::int_least64_t default_timeout{-500000}; // 50 ms, in 100 ns units
	::fast_io::win32::nt::io_status_block io_status;
	void *handle{};
	auto const status{::fast_io::win32::nt::nt_create_named_pipe_file<family == nt_family::zw>(
		__builtin_addressof(handle),
		desired_access,
		__builtin_addressof(object_attributes),
		__builtin_addressof(io_status),
		share_access,
		0x00000003u, // FILE_OPEN_IF
		create_options,
		pipe_type,
		read_mode,
		0x00000000u,                            // FILE_PIPE_QUEUE_OPERATION
		static_cast<::std::uint_least32_t>(-1), // PIPE_UNLIMITED_INSTANCES
		0x4000u,
		0x4000u,
		__builtin_addressof(default_timeout))};
	if (status) [[unlikely]]
	{
		throw_nt_error(status);
	}
	return handle;
}

template <nt_family family>
struct nt_family_create_named_pipe_ipc_server_parameter
{
	ipc_mode im{};
	inline void *operator()(char16_t const *filename, ::std::size_t filename_size) const
	{
		return nt_family_create_named_pipe_ipc_server_impl<family>(filename, filename_size, im);
	}
};

template <nt_family family, typename T>
	requires((family == nt_family::nt || family == nt_family::zw) &&
			 ::fast_io::constructible_to_os_c_str<T>)
inline void *nt_create_named_pipe_ipc_server_impl(T const &t, ipc_mode im)
{
	return ::fast_io::nt_api_common(t, nt_family_create_named_pipe_ipc_server_parameter<family>{im});
}

template <nt_family family>
	requires(family == nt_family::nt || family == nt_family::zw)
inline ::std::uint_least32_t nt_family_named_pipe_fs_control_impl(
	void *pipe_handle, ::std::uint_least32_t control_code)
{
	::fast_io::win32::nt::io_status_block io_status{};
	auto status{::fast_io::win32::nt::nt_fs_control_file<family == nt_family::zw>(
		pipe_handle,
		nullptr,
		nullptr,
		nullptr,
		__builtin_addressof(io_status),
		control_code,
		nullptr,
		0u,
		nullptr,
		0u)};
	if (status == 0x00000103u) // STATUS_PENDING
	{
		status = ::fast_io::win32::nt::nt_wait_for_single_object<family == nt_family::zw>(
			pipe_handle, false, nullptr);
		if (status) [[unlikely]]
		{
			throw_nt_error(status);
		}
		status = io_status.Status;
	}
	return status;
}

template <nt_family family>
	requires(family == nt_family::nt || family == nt_family::zw)
inline void nt_family_named_pipe_ipc_server_wait_for_connect_impl(void *pipe_handle)
{
	auto const status{nt_family_named_pipe_fs_control_impl<family>(pipe_handle, 0x00110008u)}; // FSCTL_PIPE_LISTEN
	if (status && status != 0xC00000B2u /*STATUS_PIPE_CONNECTED*/) [[unlikely]]
	{
		throw_nt_error(status);
	}
}

template <nt_family family>
	requires(family == nt_family::nt || family == nt_family::zw)
inline void nt_family_named_pipe_ipc_server_disconnect_impl(void *pipe_handle)
{
	auto const status{
		nt_family_named_pipe_fs_control_impl<family>(pipe_handle, 0x00110004u)}; // FSCTL_PIPE_DISCONNECT
	if (status) [[unlikely]]
	{
		throw_nt_error(status);
	}
}

template <nt_family family>
	requires(family == nt_family::nt || family == nt_family::zw)
inline void *nt_family_ipc_named_pipe_client_connect_impl(
	nt_named_pipe_char_type const *server_name, ::std::size_t server_name_size, ::fast_io::ipc_mode mode)
{
	auto pipe_name{nt_named_pipe_path(server_name, server_name_size)};
	auto const pipe_name_bytes{
		::fast_io::win32::nt::details::strlen_to_nt_filename_bytes(pipe_name.size())};
	::fast_io::win32::nt::unicode_string pipe_name_unicode{
		.Length = pipe_name_bytes,
		.MaximumLength = pipe_name_bytes,
		.Buffer = pipe_name.data()};
	::fast_io::win32::nt::object_attributes object_attributes{
		.Length = sizeof(::fast_io::win32::nt::object_attributes),
		.RootDirectory = nullptr,
		.ObjectName = __builtin_addressof(pipe_name_unicode),
		.Attributes = 0,
		.SecurityDescriptor = nullptr,
		.SecurityQualityOfService = nullptr};

	::std::uint_least32_t desired_access{0x00100000u | 0x00000080u | 0x00000100u};
	// SYNCHRONIZE | FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES
	if ((mode & ::fast_io::ipc_mode::in) == ::fast_io::ipc_mode::in)
	{
		desired_access |= 0x00020000u | 0x00000001u; // READ_CONTROL | FILE_READ_DATA
	}
	if ((mode & ::fast_io::ipc_mode::out) == ::fast_io::ipc_mode::out)
	{
		desired_access |= 0x00020000u | 0x00000002u | 0x00000004u;
		// READ_CONTROL | FILE_WRITE_DATA | FILE_APPEND_DATA
	}

	::std::uint_least32_t create_options{0x00200000u | 0x00000004u | 0x00000040u};
	// FILE_OPEN_REPARSE_POINT | FILE_SEQUENTIAL_ONLY | FILE_NON_DIRECTORY_FILE
	if ((mode & ::fast_io::ipc_mode::sync) != ::fast_io::ipc_mode::none)
	{
		create_options |= 0x00000002u; // FILE_WRITE_THROUGH
	}
	if ((mode & ::fast_io::ipc_mode::no_block) == ::fast_io::ipc_mode::none)
	{
		create_options |= 0x00000020u; // FILE_SYNCHRONOUS_IO_NONALERT
	}

	::fast_io::win32::nt::io_status_block io_status;
	void *handle{};
	auto const status{::fast_io::win32::nt::nt_create_file<family == nt_family::zw>(
		__builtin_addressof(handle),
		desired_access,
		__builtin_addressof(object_attributes),
		__builtin_addressof(io_status),
		nullptr,
		0x00000080u, // FILE_ATTRIBUTE_NORMAL
		0x00000003u, // FILE_SHARE_READ | FILE_SHARE_WRITE
		0x00000001u, // FILE_OPEN
		create_options,
		nullptr,
		0u)};
	if (status) [[unlikely]]
	{
		throw_nt_error(status);
	}
	return handle;
}

template <nt_family family>
struct nt_family_create_named_pipe_ipc_client_parameter
{
	ipc_mode im{};
	inline void *operator()(char16_t const *filename, ::std::size_t filename_size) const
	{
		return nt_family_ipc_named_pipe_client_connect_impl<family>(filename, filename_size, im);
	}
};

template <nt_family family, typename T>
	requires((family == nt_family::nt || family == nt_family::zw) &&
			 ::fast_io::constructible_to_os_c_str<T>)
inline void *nt_create_named_pipe_ipc_client_impl(T const &t, ipc_mode im)
{
	return ::fast_io::nt_api_common(t, nt_family_create_named_pipe_ipc_client_parameter<family>{im});
}

} // namespace win32::nt::details

template <nt_family family, ::std::integral ch_type>
	requires(family == nt_family::nt || family == nt_family::zw)
using basic_nt_family_named_pipe_ipc_server_observer = basic_nt_family_io_observer<family, ch_type>;

template <nt_family family, ::std::integral ch_type>
	requires(family == nt_family::nt || family == nt_family::zw)
class basic_nt_family_named_pipe_ipc_server
	: public basic_nt_family_named_pipe_ipc_server_observer<family, ch_type>
{
public:
	using typename basic_nt_family_named_pipe_ipc_server_observer<family, ch_type>::char_type;
	using typename basic_nt_family_named_pipe_ipc_server_observer<family, ch_type>::input_char_type;
	using typename basic_nt_family_named_pipe_ipc_server_observer<family, ch_type>::output_char_type;
	using typename basic_nt_family_named_pipe_ipc_server_observer<family, ch_type>::native_handle_type;
	using basic_nt_family_named_pipe_ipc_server_observer<family, ch_type>::native_handle;

	inline explicit constexpr basic_nt_family_named_pipe_ipc_server() noexcept = default;

	inline constexpr basic_nt_family_named_pipe_ipc_server(
		basic_nt_family_named_pipe_ipc_server_observer<family, ch_type>) noexcept = delete;
	inline constexpr basic_nt_family_named_pipe_ipc_server &operator=(
		basic_nt_family_named_pipe_ipc_server_observer<family, ch_type>) noexcept = delete;

	inline basic_nt_family_named_pipe_ipc_server(basic_nt_family_named_pipe_ipc_server const &other)
		: basic_nt_family_named_pipe_ipc_server_observer<family, ch_type>{
			  ::fast_io::win32::nt::details::nt_dup_impl<family == nt_family::zw>(other.handle)}
	{}

	inline basic_nt_family_named_pipe_ipc_server &operator=(
		basic_nt_family_named_pipe_ipc_server const &other)
	{
		if (__builtin_addressof(other) == this) [[unlikely]]
		{
			return *this;
		}
		this->handle = ::fast_io::win32::nt::details::nt_dup2_impl<family == nt_family::zw>(
			other.handle, this->handle);
		return *this;
	}

	inline basic_nt_family_named_pipe_ipc_server(
		basic_nt_family_named_pipe_ipc_server &&__restrict other) noexcept
		: basic_nt_family_named_pipe_ipc_server_observer<family, ch_type>{other.release()}
	{}

	inline basic_nt_family_named_pipe_ipc_server &operator=(
		basic_nt_family_named_pipe_ipc_server &&__restrict other) noexcept
	{
		if (__builtin_addressof(other) == this) [[unlikely]]
		{
			return *this;
		}
		if (*this) [[likely]]
		{
			::fast_io::win32::nt::nt_close<family == nt_family::zw>(this->handle);
		}
		this->handle = other.handle;
		other.handle = nullptr;
		return *this;
	}

	inline void reset(native_handle_type newhandle = {}) noexcept
	{
		if (*this) [[likely]]
		{
			::fast_io::win32::nt::nt_close<family == nt_family::zw>(this->handle);
		}
		this->handle = newhandle;
	}

	inline void close()
	{
		if (*this) [[likely]]
		{
			auto const status{
				::fast_io::win32::nt::nt_close<family == nt_family::zw>(this->handle)};
			this->handle = nullptr;
			if (status) [[unlikely]]
			{
				throw_nt_error(status);
			}
		}
	}

	template <typename native_hd>
		requires ::std::same_as<native_handle_type, ::std::remove_cvref_t<native_hd>>
	inline explicit constexpr basic_nt_family_named_pipe_ipc_server(native_hd handle1) noexcept
		: basic_nt_family_named_pipe_ipc_server_observer<family, ch_type>{handle1}
	{}

	inline basic_nt_family_named_pipe_ipc_server(
		io_dup_t, basic_nt_family_named_pipe_ipc_server_observer<family, ch_type> observer)
		: basic_nt_family_named_pipe_ipc_server_observer<family, ch_type>{
			  ::fast_io::win32::nt::details::nt_dup_impl<family == nt_family::zw>(observer.handle)}
	{}

	template <::fast_io::constructible_to_os_c_str T>
	inline explicit basic_nt_family_named_pipe_ipc_server(T const &server_name, ipc_mode im)
		: basic_nt_family_named_pipe_ipc_server_observer<family, char_type>{
			  ::fast_io::win32::nt::details::nt_create_named_pipe_ipc_server_impl<family>(server_name, im)}
	{}

	inline ~basic_nt_family_named_pipe_ipc_server()
	{
		if (*this) [[likely]]
		{
			::fast_io::win32::nt::nt_close<family == nt_family::zw>(this->handle);
		}
	}
};

template <nt_family family, ::std::integral ch_type>
	requires(family == nt_family::nt || family == nt_family::zw)
using basic_nt_family_named_pipe_ipc_client_observer = basic_nt_family_io_observer<family, ch_type>;

template <nt_family family, ::std::integral ch_type>
	requires(family == nt_family::nt || family == nt_family::zw)
class basic_nt_family_named_pipe_ipc_client
	: public basic_nt_family_named_pipe_ipc_client_observer<family, ch_type>
{
public:
	using typename basic_nt_family_named_pipe_ipc_client_observer<family, ch_type>::char_type;
	using typename basic_nt_family_named_pipe_ipc_client_observer<family, ch_type>::input_char_type;
	using typename basic_nt_family_named_pipe_ipc_client_observer<family, ch_type>::output_char_type;
	using typename basic_nt_family_named_pipe_ipc_client_observer<family, ch_type>::native_handle_type;
	using basic_nt_family_named_pipe_ipc_client_observer<family, ch_type>::native_handle;

	inline explicit constexpr basic_nt_family_named_pipe_ipc_client() noexcept = default;

	inline constexpr basic_nt_family_named_pipe_ipc_client(
		basic_nt_family_named_pipe_ipc_client_observer<family, ch_type>) noexcept = delete;
	inline constexpr basic_nt_family_named_pipe_ipc_client &operator=(
		basic_nt_family_named_pipe_ipc_client_observer<family, ch_type>) noexcept = delete;

	inline basic_nt_family_named_pipe_ipc_client(basic_nt_family_named_pipe_ipc_client const &other)
		: basic_nt_family_named_pipe_ipc_client_observer<family, ch_type>{
			  ::fast_io::win32::nt::details::nt_dup_impl<family == nt_family::zw>(other.handle)}
	{}

	inline basic_nt_family_named_pipe_ipc_client &operator=(
		basic_nt_family_named_pipe_ipc_client const &other)
	{
		if (__builtin_addressof(other) == this) [[unlikely]]
		{
			return *this;
		}
		this->handle = ::fast_io::win32::nt::details::nt_dup2_impl<family == nt_family::zw>(
			other.handle, this->handle);
		return *this;
	}

	inline basic_nt_family_named_pipe_ipc_client(
		basic_nt_family_named_pipe_ipc_client &&__restrict other) noexcept
		: basic_nt_family_named_pipe_ipc_client_observer<family, ch_type>{other.release()}
	{}

	inline basic_nt_family_named_pipe_ipc_client &operator=(
		basic_nt_family_named_pipe_ipc_client &&__restrict other) noexcept
	{
		if (__builtin_addressof(other) == this) [[unlikely]]
		{
			return *this;
		}
		if (*this) [[likely]]
		{
			::fast_io::win32::nt::nt_close<family == nt_family::zw>(this->handle);
		}
		this->handle = other.handle;
		other.handle = nullptr;
		return *this;
	}

	inline void reset(native_handle_type newhandle = {}) noexcept
	{
		if (*this) [[likely]]
		{
			::fast_io::win32::nt::nt_close<family == nt_family::zw>(this->handle);
		}
		this->handle = newhandle;
	}

	inline void close()
	{
		if (*this) [[likely]]
		{
			auto const status{
				::fast_io::win32::nt::nt_close<family == nt_family::zw>(this->handle)};
			this->handle = nullptr;
			if (status) [[unlikely]]
			{
				throw_nt_error(status);
			}
		}
	}

	template <typename native_hd>
		requires ::std::same_as<native_handle_type, ::std::remove_cvref_t<native_hd>>
	inline explicit constexpr basic_nt_family_named_pipe_ipc_client(native_hd handle1) noexcept
		: basic_nt_family_named_pipe_ipc_client_observer<family, ch_type>{handle1}
	{}

	inline basic_nt_family_named_pipe_ipc_client(
		io_dup_t, basic_nt_family_named_pipe_ipc_client_observer<family, ch_type> observer)
		: basic_nt_family_named_pipe_ipc_client_observer<family, ch_type>{
			  ::fast_io::win32::nt::details::nt_dup_impl<family == nt_family::zw>(observer.handle)}
	{}

	template <::fast_io::constructible_to_os_c_str T>
	inline explicit basic_nt_family_named_pipe_ipc_client(T const &client_name, ipc_mode im)
		: basic_nt_family_named_pipe_ipc_client_observer<family, char_type>{
			  ::fast_io::win32::nt::details::nt_create_named_pipe_ipc_client_impl<family>(client_name, im)}
	{}

	inline ~basic_nt_family_named_pipe_ipc_client()
	{
		if (*this) [[likely]]
		{
			::fast_io::win32::nt::nt_close<family == nt_family::zw>(this->handle);
		}
	}
};

template <nt_family server_family, ::std::integral server_ch_type,
		  nt_family client_family = nt_family::nt, ::std::integral client_ch_type = char>
	requires((server_family == nt_family::nt || server_family == nt_family::zw) &&
			 (client_family == nt_family::nt || client_family == nt_family::zw))
inline basic_nt_family_named_pipe_ipc_client<client_family, client_ch_type> wait_for_connect(
	basic_nt_family_named_pipe_ipc_server_observer<server_family, server_ch_type> server)
{
	::fast_io::win32::nt::details::nt_family_named_pipe_ipc_server_wait_for_connect_impl<server_family>(
		server.handle);
	return basic_nt_family_named_pipe_ipc_client<client_family, client_ch_type>{};
}

template <nt_family server_family, ::std::integral server_ch_type,
		  nt_family client_family = nt_family::nt, ::std::integral client_ch_type = char>
	requires((server_family == nt_family::nt || server_family == nt_family::zw) &&
			 (client_family == nt_family::nt || client_family == nt_family::zw))
inline void accept_connect(
	basic_nt_family_named_pipe_ipc_server_observer<server_family, server_ch_type>,
	basic_nt_family_named_pipe_ipc_client_observer<client_family, client_ch_type>,
	bool)
{}

template <nt_family server_family, ::std::integral server_ch_type>
	requires(server_family == nt_family::nt || server_family == nt_family::zw)
inline void disconnect(
	basic_nt_family_named_pipe_ipc_server_observer<server_family, server_ch_type> server) noexcept
{
	::fast_io::win32::nt::details::nt_family_named_pipe_ipc_server_disconnect_impl<server_family>(
		server.handle);
}

template <::std::integral ch_type>
using basic_nt_named_pipe_ipc_server_observer =
	basic_nt_family_named_pipe_ipc_server_observer<nt_family::nt, ch_type>;
template <::std::integral ch_type>
using basic_zw_named_pipe_ipc_server_observer =
	basic_nt_family_named_pipe_ipc_server_observer<nt_family::zw, ch_type>;

using nt_named_pipe_ipc_server_observer = basic_nt_named_pipe_ipc_server_observer<char>;
using wnt_named_pipe_ipc_server_observer = basic_nt_named_pipe_ipc_server_observer<wchar_t>;
using u8nt_named_pipe_ipc_server_observer = basic_nt_named_pipe_ipc_server_observer<char8_t>;
using u16nt_named_pipe_ipc_server_observer = basic_nt_named_pipe_ipc_server_observer<char16_t>;
using u32nt_named_pipe_ipc_server_observer = basic_nt_named_pipe_ipc_server_observer<char32_t>;

using zw_named_pipe_ipc_server_observer = basic_zw_named_pipe_ipc_server_observer<char>;
using wzw_named_pipe_ipc_server_observer = basic_zw_named_pipe_ipc_server_observer<wchar_t>;
using u8zw_named_pipe_ipc_server_observer = basic_zw_named_pipe_ipc_server_observer<char8_t>;
using u16zw_named_pipe_ipc_server_observer = basic_zw_named_pipe_ipc_server_observer<char16_t>;
using u32zw_named_pipe_ipc_server_observer = basic_zw_named_pipe_ipc_server_observer<char32_t>;

template <::std::integral ch_type>
using basic_nt_named_pipe_ipc_server =
	basic_nt_family_named_pipe_ipc_server<nt_family::nt, ch_type>;
template <::std::integral ch_type>
using basic_zw_named_pipe_ipc_server =
	basic_nt_family_named_pipe_ipc_server<nt_family::zw, ch_type>;

using nt_named_pipe_ipc_server = basic_nt_named_pipe_ipc_server<char>;
using wnt_named_pipe_ipc_server = basic_nt_named_pipe_ipc_server<wchar_t>;
using u8nt_named_pipe_ipc_server = basic_nt_named_pipe_ipc_server<char8_t>;
using u16nt_named_pipe_ipc_server = basic_nt_named_pipe_ipc_server<char16_t>;
using u32nt_named_pipe_ipc_server = basic_nt_named_pipe_ipc_server<char32_t>;

using zw_named_pipe_ipc_server = basic_zw_named_pipe_ipc_server<char>;
using wzw_named_pipe_ipc_server = basic_zw_named_pipe_ipc_server<wchar_t>;
using u8zw_named_pipe_ipc_server = basic_zw_named_pipe_ipc_server<char8_t>;
using u16zw_named_pipe_ipc_server = basic_zw_named_pipe_ipc_server<char16_t>;
using u32zw_named_pipe_ipc_server = basic_zw_named_pipe_ipc_server<char32_t>;

template <::std::integral ch_type>
using basic_nt_named_pipe_ipc_client_observer =
	basic_nt_family_named_pipe_ipc_client_observer<nt_family::nt, ch_type>;
template <::std::integral ch_type>
using basic_zw_named_pipe_ipc_client_observer =
	basic_nt_family_named_pipe_ipc_client_observer<nt_family::zw, ch_type>;

using nt_named_pipe_ipc_client_observer = basic_nt_named_pipe_ipc_client_observer<char>;
using wnt_named_pipe_ipc_client_observer = basic_nt_named_pipe_ipc_client_observer<wchar_t>;
using u8nt_named_pipe_ipc_client_observer = basic_nt_named_pipe_ipc_client_observer<char8_t>;
using u16nt_named_pipe_ipc_client_observer = basic_nt_named_pipe_ipc_client_observer<char16_t>;
using u32nt_named_pipe_ipc_client_observer = basic_nt_named_pipe_ipc_client_observer<char32_t>;

using zw_named_pipe_ipc_client_observer = basic_zw_named_pipe_ipc_client_observer<char>;
using wzw_named_pipe_ipc_client_observer = basic_zw_named_pipe_ipc_client_observer<wchar_t>;
using u8zw_named_pipe_ipc_client_observer = basic_zw_named_pipe_ipc_client_observer<char8_t>;
using u16zw_named_pipe_ipc_client_observer = basic_zw_named_pipe_ipc_client_observer<char16_t>;
using u32zw_named_pipe_ipc_client_observer = basic_zw_named_pipe_ipc_client_observer<char32_t>;

template <::std::integral ch_type>
using basic_nt_named_pipe_ipc_client =
	basic_nt_family_named_pipe_ipc_client<nt_family::nt, ch_type>;
template <::std::integral ch_type>
using basic_zw_named_pipe_ipc_client =
	basic_nt_family_named_pipe_ipc_client<nt_family::zw, ch_type>;

using nt_named_pipe_ipc_client = basic_nt_named_pipe_ipc_client<char>;
using wnt_named_pipe_ipc_client = basic_nt_named_pipe_ipc_client<wchar_t>;
using u8nt_named_pipe_ipc_client = basic_nt_named_pipe_ipc_client<char8_t>;
using u16nt_named_pipe_ipc_client = basic_nt_named_pipe_ipc_client<char16_t>;
using u32nt_named_pipe_ipc_client = basic_nt_named_pipe_ipc_client<char32_t>;

using zw_named_pipe_ipc_client = basic_zw_named_pipe_ipc_client<char>;
using wzw_named_pipe_ipc_client = basic_zw_named_pipe_ipc_client<wchar_t>;
using u8zw_named_pipe_ipc_client = basic_zw_named_pipe_ipc_client<char8_t>;
using u16zw_named_pipe_ipc_client = basic_zw_named_pipe_ipc_client<char16_t>;
using u32zw_named_pipe_ipc_client = basic_zw_named_pipe_ipc_client<char32_t>;

namespace freestanding
{

template <nt_family family, ::std::integral char_type>
struct is_trivially_copyable_or_relocatable<basic_nt_family_named_pipe_ipc_server<family, char_type>>
{
	inline static constexpr bool value = true;
};

template <nt_family family, ::std::integral char_type>
struct is_zero_default_constructible<basic_nt_family_named_pipe_ipc_server<family, char_type>>
{
	inline static constexpr bool value = true;
};

template <nt_family family, ::std::integral char_type>
struct is_trivially_copyable_or_relocatable<basic_nt_family_named_pipe_ipc_client<family, char_type>>
{
	inline static constexpr bool value = true;
};

template <nt_family family, ::std::integral char_type>
struct is_zero_default_constructible<basic_nt_family_named_pipe_ipc_client<family, char_type>>
{
	inline static constexpr bool value = true;
};

} // namespace freestanding

} // namespace fast_io
