#pragma once

#include "../mode.h"

namespace fast_io
{

enum class nt_alpc_ipc_message_type : ::std::uint_least16_t
{
	none,
	request,
	reply,
	datagram,
	lost_reply,
	port_closed,
	client_died,
	exception,
	debug_event,
	error_event,
	connection_request,
	connection_reply,
	cancel,
	legacy_connection_reply
};

struct nt_alpc_ipc_message
{
	::fast_io::containers::vector<::std::byte, ::fast_io::native_global_allocator> bytes{};
	::fast_io::win32::nt::client_id sender{};
	// On a server listener this is the stable context of the accepted connection that delivered the message.
	void *port_context{};
	::std::uint_least32_t message_id{};
	::std::uint_least32_t callback_id{};
	::std::uint_least16_t type{};
};

inline constexpr nt_alpc_ipc_message_type alpc_message_type(nt_alpc_ipc_message const &message) noexcept
{
	return static_cast<nt_alpc_ipc_message_type>(message.type & 0xffu);
}

inline ::std::size_t alpc_max_message_size() noexcept
{
	auto const native_limit{static_cast<::std::size_t>(::fast_io::win32::nt::AlpcMaxAllowedMessageLength())};
	auto const header_limit{static_cast<::std::size_t>(::std::numeric_limits<::std::uint_least16_t>::max())};
	auto const total_limit{native_limit < header_limit ? native_limit : header_limit};
	return total_limit > sizeof(::fast_io::win32::nt::port_message) ? total_limit - sizeof(::fast_io::win32::nt::port_message) : 0u;
}

struct nt_alpc_ipc_completion
{
	::std::size_t completion_key{};
	::std::uint_least32_t transferred{};
	void *overlapped{};
};

namespace win32::nt::details
{
// https://hfiref0x.github.io/X86_64/NT6_syscalls.html
// nt6x alpc

using nt_alpc_char_type = char16_t;

using nt_alpc_internal_char_type = char16_t;

using nt_alpc_internal_str = ::fast_io::containers::basic_string<nt_alpc_internal_char_type, ::fast_io::native_global_allocator>;

using nt_alpc_internal_tlc_str = ::fast_io::containers::basic_string<nt_alpc_internal_char_type, ::fast_io::native_thread_local_allocator>;

using nt_alpc_internal_strvw = ::fast_io::containers::basic_string_view<nt_alpc_internal_char_type>;

template <::std::integral ch_type>
using nt_alpc_communication_tlc_strvw = ::fast_io::containers::basic_string_view<ch_type>;

template <typename... Args>
constexpr inline nt_alpc_internal_str concat_nt_alpc_internal_str(Args &&...args)
{
	// Concat, rather than a dummy print destination, owns both source normalization and the selected string adapter.
	// Passing the named arguments preserves this wrapper's established lvalue observation without normalizing twice.
	return ::fast_io::basic_general_concat_checked<
		false, nt_alpc_internal_char_type, nt_alpc_internal_str>(args...);
}

template <typename... Args>
constexpr inline nt_alpc_internal_tlc_str concat_nt_alpc_internal_tlc_str(Args &&...args)
{
	// The thread-local result uses the identical checked concat boundary and therefore the identical executable proof.
	return ::fast_io::basic_general_concat_checked<
		false, nt_alpc_internal_char_type, nt_alpc_internal_tlc_str>(args...);
}

struct nt_alpc_connect_handle
{
	::fast_io::win32::nt::client_id cid;
	::std::uint_least32_t mid;
};

using nt_alpc_byte_vector = ::fast_io::containers::vector<::std::byte, ::fast_io::native_global_allocator>;

enum class nt_alpc_status : unsigned
{
	none,
	after_wait_for_connect,
	after_connect
};

struct nt_alpc_message_attribute_guard
{
	::fast_io::win32::nt::alpc_message_attributes *message_attribute{};

	using alpc_message_alloc = ::fast_io::native_global_allocator;

	nt_alpc_message_attribute_guard() noexcept = default;

	nt_alpc_message_attribute_guard(::fast_io::win32::nt::alpc_message_attributes *ma) noexcept
		: message_attribute{ma}
	{
	}

	nt_alpc_message_attribute_guard(nt_alpc_message_attribute_guard const &) = delete;
	nt_alpc_message_attribute_guard &operator=(nt_alpc_message_attribute_guard const &) = delete;

	inline constexpr nt_alpc_message_attribute_guard(nt_alpc_message_attribute_guard &&other) noexcept
	{
		message_attribute = other.message_attribute;
		other.message_attribute = nullptr;
	}

	inline nt_alpc_message_attribute_guard &operator=(nt_alpc_message_attribute_guard &&other) noexcept
	{
		if (message_attribute)
		{
			alpc_message_alloc::deallocate(message_attribute);
		}

		message_attribute = other.message_attribute;
		other.message_attribute = nullptr;

		return *this;
	}

	inline ~nt_alpc_message_attribute_guard()
	{
		close();
	}

	inline void close()
	{
		alpc_message_alloc::deallocate(message_attribute);
		message_attribute = nullptr;
	}
};

template <nt_family family>
struct nt_alpc_handle
{
	using alpc_message_alloc = ::fast_io::native_global_allocator;

	void *port_handle{};
	void *section_handle{};
	::fast_io::win32::nt::alpc_message_attributes *message_attribute{};
	// view section
	::std::byte *view_begin{};
	::std::byte *view_end{};
	// id
	nt_alpc_connect_handle cid{};
	nt_alpc_byte_vector byte_vector{};
	nt_alpc_byte_vector connection_reply{};
	// status
	nt_alpc_status status{};
	::fast_io::ipc_mode mode{};
	bool communication_port{};
	bool server_side_communication_port{};
	bool connection_reply_ready{};

	nt_alpc_handle() noexcept = default;

	nt_alpc_handle(nt_alpc_handle const &) = delete;
	nt_alpc_handle &operator=(nt_alpc_handle const &) = delete;
	nt_alpc_handle(nt_alpc_handle &&) = delete;
	nt_alpc_handle &operator=(nt_alpc_handle &&) = delete;

	inline ~nt_alpc_handle()
	{
		close_noexcept();
	}

	inline void close_noexcept() noexcept
	{
		constexpr bool zw{family == nt_family::zw};

		if (port_handle)
		{
			if (communication_port)
			{
				::fast_io::win32::nt::nt_alpc_disconnect_port<zw>(port_handle, 0);
			}
			::fast_io::win32::nt::nt_close<zw>(port_handle);
			port_handle = nullptr;
		}

		section_handle = nullptr;

		alpc_message_alloc::deallocate(message_attribute);
		message_attribute = nullptr;

		view_begin = nullptr;

		view_end = nullptr;

		cid = {};

		byte_vector.clear();

		connection_reply.clear();

		status = {};

		mode = {};

		communication_port = false;

		server_side_communication_port = false;

		connection_reply_ready = false;
	}

	inline void close()
	{
		constexpr bool zw{family == nt_family::zw};

		::std::uint_least32_t first_error{};

		if (port_handle)
		{
			if (communication_port)
			{
				auto const disconnect_status{::fast_io::win32::nt::nt_alpc_disconnect_port<zw>(port_handle, 0)};
				if (disconnect_status && disconnect_status != 0xc0000037u)
				{
					first_error = disconnect_status;
				}
			}
			auto const close_status{::fast_io::win32::nt::nt_close<zw>(port_handle)};
			if (!first_error && close_status)
			{
				first_error = close_status;
			}
			port_handle = nullptr;
		}

		section_handle = nullptr;

		alpc_message_alloc::deallocate(message_attribute);
		message_attribute = nullptr;

		view_begin = nullptr;

		view_end = nullptr;

		cid = {};

		byte_vector.clear();

		connection_reply.clear();

		status = {};

		mode = {};

		communication_port = false;

		server_side_communication_port = false;

		connection_reply_ready = false;

		if (first_error) [[unlikely]]
		{
			throw_nt_error(first_error);
		}
	}
};

template <nt_family family>
using nt_alpc_handle_allocator = ::fast_io::native_typed_global_allocator<nt_alpc_handle<family>>;

template <nt_family family>
inline nt_alpc_handle<family> *nt_alpc_allocate_handle()
{
	auto ptr{nt_alpc_handle_allocator<family>::allocate(1)};
	return ::std::construct_at(ptr);
}

template <nt_family family>
inline void nt_alpc_deallocate_handle(nt_alpc_handle<family> *ptr) noexcept
{
	if (ptr)
	{
		::std::destroy_at(ptr);
		nt_alpc_handle_allocator<family>::deallocate_n(ptr, 1);
	}
}

struct nt_ipc_alpc_thread_local_heap_allocate_guard
{
	using alloc = ::fast_io::native_global_allocator;

	void *ptr{};
	inline constexpr nt_ipc_alpc_thread_local_heap_allocate_guard() noexcept = default;
	inline constexpr nt_ipc_alpc_thread_local_heap_allocate_guard(void *o_ptr) noexcept
		: ptr{o_ptr} {};

	nt_ipc_alpc_thread_local_heap_allocate_guard(nt_ipc_alpc_thread_local_heap_allocate_guard const &) = delete;
	nt_ipc_alpc_thread_local_heap_allocate_guard &operator=(nt_ipc_alpc_thread_local_heap_allocate_guard const &) = delete;
	inline constexpr ~nt_ipc_alpc_thread_local_heap_allocate_guard()
	{
		clear();
	};
	inline constexpr void clear() noexcept
	{
		alloc::deallocate(ptr);
		ptr = nullptr;
	}
};

// SERVER
template <nt_family family>
inline void *nt_family_create_alpc_ipc_server_port_impl(nt_alpc_char_type const *server_name, ::std::size_t server_name_size, [[maybe_unused]] ::fast_io::ipc_mode mode)
{
	constexpr bool zw{family == nt_family::zw};

	if (::fast_io::details::is_invalid_dos_filename_with_size(server_name, server_name_size)) [[unlikely]]
	{
		throw_nt_error(3221225524);
	}

	auto temp_ipc_name_tlc_str{concat_nt_alpc_internal_tlc_str(u"\\RPC Control\\fast_io_ipc_", ::fast_io::mnp::os_c_str_with_known_size(server_name, server_name_size))};

	::fast_io::win32::nt::unicode_string us{};
	us.Buffer = const_cast<char16_t *>(temp_ipc_name_tlc_str.c_str());
	auto const temp_ipc_name_tlc_str_size_bytes{temp_ipc_name_tlc_str.size_bytes()};
	us.Length = static_cast<::std::uint_least16_t>(temp_ipc_name_tlc_str_size_bytes);
	us.MaximumLength = ::fast_io::win32::nt::details::nt_filename_bytes_check(temp_ipc_name_tlc_str_size_bytes + sizeof(char16_t));

	::fast_io::win32::nt::object_attributes oa{};
	oa.Length = sizeof(::fast_io::win32::nt::object_attributes);
	oa.ObjectName = __builtin_addressof(us);

	::fast_io::win32::nt::alpc_port_attributes apa{};
	apa.Flags = 0x80000 /*ALPC_PORTFLG_ALLOW_DUP_OBJECT*/ |
				0x20000 /*ALPC_PORTFLG_ALLOW_LPC_REQUESTS*/;
	apa.MaxMessageLength = ::fast_io::win32::nt::AlpcMaxAllowedMessageLength();

#if 0
	::fast_io::win32::nt::security_quality_of_service SecurityQos{};
	SecurityQos.ImpersonationLevel = ::fast_io::win32::nt::security_impersonation_level::SecurityIdentification; // SecurityImpersonation; // ; // ; // ;// ;
	SecurityQos.ContextTrackingMode = 0 /*SECURITY_STATIC_TRACKING*/;
	SecurityQos.Length = sizeof(SecurityQos);
	apa.SecurityQos = SecurityQos;
#endif

	void *server_port_handle;

	check_nt_status(::fast_io::win32::nt::nt_alpc_create_port<zw>(
		__builtin_addressof(server_port_handle),
		__builtin_addressof(oa),
		__builtin_addressof(apa)));

	return server_port_handle;
}

template <nt_family family>
struct nt_family_create_alpc_ipc_server_paramenter
{
	using family_char_type = char16_t;
	ipc_mode im{};
	inline void *operator()(family_char_type const *filename, ::std::size_t filename_size)
	{
		return nt_family_create_alpc_ipc_server_port_impl<family>(filename, filename_size, im);
	}
};

template <nt_family family, typename T>
	requires(::fast_io::constructible_to_os_c_str<T>)
inline void *nt_create_alpc_ipc_server_impl(T const &t, ipc_mode im)
{
	return ::fast_io::nt_api_common(t, nt_family_create_alpc_ipc_server_paramenter<family>{im});
}

template <nt_family family>
inline ::fast_io::win32::nt::alpc_message_attributes *nt_family_create_alpc_ipc_server_message_attribute_view_impl(void *__restrict server_port)
{
	constexpr bool zw{family == nt_family::zw};

	constexpr ::std::uint_least32_t message_attribute{
		0x80000000 /*ALPC_MESSAGE_SECURITY_ATTRIBUTE*/ |
		0x40000000 /*ALPC_MESSAGE_VIEW_ATTRIBUTE*/};

	auto const header_size{::fast_io::win32::nt::AlpcGetHeaderSize(message_attribute)};

	if (header_size == 0) [[unlikely]]
	{
		throw_nt_error(3221225485);
	}

	auto message_stroge{nt_alpc_handle<family>::alpc_message_alloc::allocate(header_size)}; // This function does not recycle

	using alpc_message_attributes_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= ::fast_io::win32::nt::alpc_message_attributes *;

	auto p_msg_attr_aend{reinterpret_cast<alpc_message_attributes_may_alias_ptr>(message_stroge)};

	::std::size_t real_buf_size;

	check_nt_status(::fast_io::win32::nt::AlpcInitializeMessageAttribute(
		message_attribute,                 // the MessageAttribute
		p_msg_attr_aend,                   // pointer to allocated buffer that is used to holf attributes structures
		header_size,                       // buffer that has been allocated
		__builtin_addressof(real_buf_size) // the size that would be needed (in case of the buffer allocated was too small)
		));

	::std::size_t iNextMsgAttrBufferOffset{sizeof(::fast_io::win32::nt::alpc_message_attributes)}; // 4 bytes allocated attributes + 4 bytes valid attributes

	// ALPC_MESSAGE_SECURITY_ATTRIBUTE
	::fast_io::win32::nt::security_quality_of_service SecurityQos{};
	::fast_io::win32::nt::alpc_security_attr securityAttr{};

	SecurityQos.ImpersonationLevel = ::fast_io::win32::nt::security_impersonation_level::SecurityAnonymous; // SecurityIdentification;
	SecurityQos.ContextTrackingMode = 0 /*SECURITY_STATIC_TRACKING*/;
	SecurityQos.EffectiveOnly = 0;
	SecurityQos.Length = sizeof(SecurityQos);
	securityAttr.pQOS = __builtin_addressof(SecurityQos);
	securityAttr.Flags = 0; // 0x10000;
	check_nt_status(::fast_io::win32::nt::nt_alpc_create_security_context<zw>(server_port, 0, __builtin_addressof(securityAttr)));
	::fast_io::freestanding::my_memmove(reinterpret_cast<::std::byte *>(p_msg_attr_aend) + iNextMsgAttrBufferOffset, __builtin_addressof(securityAttr), sizeof(securityAttr));
	iNextMsgAttrBufferOffset += sizeof(securityAttr);

	// ALPC_MESSAGE_VIEW_ATTRIBUTE
	::fast_io::win32::nt::alpc_data_view_attr viewAttr{};
	viewAttr.Flags = 0;         // unknown
	viewAttr.SectionHandle = 0; // Future allocation
	viewAttr.ViewBase = 0;      // Automatic assign
	viewAttr.ViewSize = 0;      // Future allocation

	// place ALPC_MESSAGE_VIEW_ATTRIBUTE structure
	::fast_io::freestanding::my_memmove(reinterpret_cast<::std::byte *>(p_msg_attr_aend) + iNextMsgAttrBufferOffset, __builtin_addressof(viewAttr), sizeof(viewAttr));
	iNextMsgAttrBufferOffset += sizeof(viewAttr);

	return p_msg_attr_aend;
}

template <nt_family family>
inline void *nt_family_add_section_view_to_alpc_ipc_server_message_attribute_view_impl(
	void *__restrict server_port_handle, ::fast_io::win32::nt::alpc_message_attributes *__restrict ama, ::std::size_t view_size)
{
	constexpr bool zw{family == nt_family::zw};

	void *section_handle;
	::std::size_t server_section_size;

	check_nt_status(::fast_io::win32::nt::nt_alpc_create_port_section<zw>(
		server_port_handle,                      //_In_ HANDLE PortHandle,
		0,                                       //_In_ ULONG Flags, unknown, 0x40000 found in rpcrt4.dll
		nullptr,                                 //_In_opt_ HANDLE SectionHandle,
		0x1000,                                  // _In_ SIZE_T SectionSize,
		__builtin_addressof(section_handle),     //_Out_ HANDLE AlpcSectionHandle,
		__builtin_addressof(server_section_size) //_Out_ PSIZE_T ActualSectionSize
		));

	::fast_io::win32::nt::alpc_data_view_attr viewAttr{};
	viewAttr.Flags = 0; // unknown
	viewAttr.SectionHandle = section_handle;
	viewAttr.ViewBase = 0; // Automatic assign
	viewAttr.ViewSize = view_size;
	check_nt_status(::fast_io::win32::nt::nt_alpc_create_section_view<zw>(
		server_port_handle,           //_In_ HANDLE PortHandle,
		0,                            // _Reserved_ ULONG Flags, unknown
		__builtin_addressof(viewAttr) //_Inout_ PALPC_DATA_VIEW_ATTR ViewAttributes
		));

	::std::size_t iNextMsgAttrBufferOffset{sizeof(::fast_io::win32::nt::alpc_message_attributes)};
	iNextMsgAttrBufferOffset += sizeof(::fast_io::win32::nt::security_quality_of_service);
	::fast_io::freestanding::my_memmove(reinterpret_cast<::std::byte *>(ama) + iNextMsgAttrBufferOffset, __builtin_addressof(viewAttr), sizeof(viewAttr));

	return section_handle;
}

struct nt_family_alpc_ipc_server_wait_for_connect_rets
{
	nt_alpc_connect_handle ch;
	::std::byte *rc;
};

template <nt_family family>
inline nt_family_alpc_ipc_server_wait_for_connect_rets nt_family_alpc_ipc_server_wait_for_connect_impl(
	void *__restrict server_pipe_handle, ::fast_io::win32::nt::alpc_message_attributes *__restrict ama,
	::std::byte *handshake_msg_beg, ::std::byte *handshake_msg_end)
{
	constexpr bool zw{family == nt_family::zw};

	::std::size_t receive_size{sizeof(::fast_io::win32::nt::port_message) + static_cast<::std::size_t>(handshake_msg_end - handshake_msg_beg)};

	auto tmp{static_cast<::fast_io::win32::nt::alpc_message *>(nt_ipc_alpc_thread_local_heap_allocate_guard::alloc::allocate(receive_size))};
	nt_ipc_alpc_thread_local_heap_allocate_guard tmp_guard{tmp};

	using port_message_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= ::fast_io::win32::nt::port_message *;

	auto port_message_p{reinterpret_cast<port_message_may_alias_ptr>(tmp)};

	check_nt_status(::fast_io::win32::nt::nt_alpc_send_wait_receive_port<zw>(
		server_pipe_handle,
		0,                                 // no flags
		nullptr,                           // SendMessage
		nullptr,                           // SendMessageAttributes
		port_message_p,                    // ReceiveBuffer
		__builtin_addressof(receive_size), // BufferLength
		ama,                               // ReceiveMessageAttributes
		nullptr                            // no timeout
		));

	auto const actual_receive_size{static_cast<::std::size_t>(port_message_p->u1.s1.DataLength)};

	::fast_io::freestanding::my_memcpy(handshake_msg_beg, tmp->PortMessage, actual_receive_size);

	return {{port_message_p->ClientId, port_message_p->MessageId}, static_cast<::std::byte *>(handshake_msg_beg + actual_receive_size)};
}

template <nt_family family>
inline nt_alpc_connect_handle nt_family_alpc_ipc_server_wait_for_connect_and_write_bv_impl(
	void *__restrict server_pipe_handle, ::fast_io::win32::nt::alpc_message_attributes *__restrict ama,
	nt_alpc_byte_vector &connect_recv_message)
{
	constexpr bool zw{family == nt_family::zw};

	::std::size_t receive_size{};

	::fast_io::win32::nt::port_message pm{};

	// get receive size
	auto status{::fast_io::win32::nt::nt_alpc_send_wait_receive_port<zw>(
		server_pipe_handle,
		0,                                 // no flags
		nullptr,                           // SendMessage
		nullptr,                           // SendMessageAttributes
		__builtin_addressof(pm),           // ReceiveBuffer
		__builtin_addressof(receive_size), // BufferLength
		ama,                               // ReceiveMessageAttributes
		nullptr                            // no timeout
		)};

	if (status == 0xc0000023)
	{
		auto tmp{static_cast<::fast_io::win32::nt::alpc_message *>(nt_ipc_alpc_thread_local_heap_allocate_guard::alloc::allocate(receive_size))};
		nt_ipc_alpc_thread_local_heap_allocate_guard tmp_guard{tmp};

		using port_message_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= ::fast_io::win32::nt::port_message *;

		auto port_message_p{reinterpret_cast<port_message_may_alias_ptr>(tmp)};

		check_nt_status(::fast_io::win32::nt::nt_alpc_send_wait_receive_port<zw>(
			server_pipe_handle,
			0,                                 // no flags
			nullptr,                           // SendMessage
			nullptr,                           // SendMessageAttributes
			port_message_p,                    // ReceiveBuffer
			__builtin_addressof(receive_size), // BufferLength
			ama,                               // ReceiveMessageAttributes
			nullptr                            // no timeout
			));

		auto const recv_message_sizes{static_cast<::std::size_t>(port_message_p->u1.s1.DataLength)};
		connect_recv_message.clear();
		connect_recv_message.resize(recv_message_sizes);
		if (recv_message_sizes)
		{
			::fast_io::freestanding::my_memcpy(connect_recv_message.data(), tmp->PortMessage, recv_message_sizes);
		}
		if ((port_message_p->u2.s2.Type & 0xffu) != 10u) [[unlikely]]
		{
			throw_nt_error(0xc0000701u);
		}

		return {port_message_p->ClientId, port_message_p->MessageId};
	}
	else if (status)
	{
		throw_nt_error(status);
	}
	else
	{
		// no message data
		connect_recv_message.clear();
		if ((pm.u2.s2.Type & 0xffu) != 10u) [[unlikely]]
		{
			throw_nt_error(0xc0000701u);
		}

		return {pm.ClientId, pm.MessageId};
	}
}

template <nt_family family>
inline void *nt_family_alpc_ipc_server_accept_connect_and_send_impl(
	void *__restrict server_pipe_handle, nt_alpc_connect_handle connect_handle, bool accept,
	::fast_io::win32::nt::alpc_message_attributes *__restrict ama,
	::std::byte const *handshake_msg_beg, ::std::byte const *handshake_msg_end, ::fast_io::ipc_mode mode,
	void *port_context)
{
	constexpr bool zw{family == nt_family::zw};

	void *client_port_handle{};

	::fast_io::win32::nt::alpc_port_attributes apa{};
	apa.Flags = 0x80000 /*ALPC_PORTFLG_ALLOW_DUP_OBJECT*/ |
				0x20000 /*ALPC_PORTFLG_ALLOW_LPC_REQUESTS*/;
	apa.MaxMessageLength = ::fast_io::win32::nt::AlpcMaxAllowedMessageLength();

	::std::size_t const message_size{handshake_msg_beg ? static_cast<::std::size_t>(handshake_msg_end - handshake_msg_beg) : 0u};
	::std::size_t send_size{sizeof(::fast_io::win32::nt::port_message) + message_size};

	auto tmp{static_cast<::fast_io::win32::nt::alpc_message *>(nt_ipc_alpc_thread_local_heap_allocate_guard::alloc::allocate(send_size))};
	nt_ipc_alpc_thread_local_heap_allocate_guard tmp_guard{tmp};

	::fast_io::freestanding::my_memset(tmp, 0, sizeof(::fast_io::win32::nt::port_message));

	tmp->PortHeader.u1.s1.DataLength = static_cast<::std::uint_least16_t>(message_size);
	tmp->PortHeader.u1.s1.TotalLength = static_cast<::std::uint_least16_t>(send_size);
	tmp->PortHeader.MessageId = connect_handle.mid;

	if (handshake_msg_beg)
	{
		::fast_io::freestanding::my_memcpy(tmp->PortMessage, handshake_msg_beg, message_size);
	}

	using port_message_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= ::fast_io::win32::nt::port_message *;

	auto port_message_p{reinterpret_cast<port_message_may_alias_ptr>(tmp)};

	check_nt_status(::fast_io::win32::nt::nt_alpc_accept_connect_port<zw>(
		__builtin_addressof(client_port_handle),
		server_pipe_handle,
		(mode & ::fast_io::ipc_mode::sync) == ::fast_io::ipc_mode::sync ? 0x20000u /*ALPC_SYNC_CONNECTION*/ : 0u,
		nullptr,
		__builtin_addressof(apa),
		port_context,
		port_message_p,
		ama,
		static_cast<int>(accept)));

	return client_port_handle;
}

template <nt_family family>
inline void nt_family_alpc_ipc_server_disconnect_impl(void *__restrict client_pipe_handle)
{
	constexpr bool zw{family == nt_family::zw};

	check_nt_status(::fast_io::win32::nt::nt_alpc_disconnect_port<zw>(client_pipe_handle, 0));
}

// CLIENT
template <nt_family family>
inline ::fast_io::win32::nt::alpc_message_attributes *nt_family_create_alpc_ipc_client_message_attribute_view_impl()
{
	// constexpr bool zw{family == nt_family::zw};

	constexpr ::std::uint_least32_t message_attribute{
		0x80000000 /*ALPC_MESSAGE_SECURITY_ATTRIBUTE*/ |
		0x40000000 /*ALPC_MESSAGE_VIEW_ATTRIBUTE*/
	};

	auto const header_size{::fast_io::win32::nt::AlpcGetHeaderSize(message_attribute)};

	if (header_size == 0) [[unlikely]]
	{
		throw_nt_error(3221225485);
	}

	auto message_stroge{nt_alpc_handle<family>::alpc_message_alloc::allocate(header_size)}; // This function does not recycle

	using alpc_message_attributes_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= ::fast_io::win32::nt::alpc_message_attributes *;

	auto p_msg_attr_aend{reinterpret_cast<alpc_message_attributes_may_alias_ptr>(message_stroge)};

	::std::size_t real_buf_size;

	check_nt_status(::fast_io::win32::nt::AlpcInitializeMessageAttribute(
		message_attribute,                 // the MessageAttribute
		p_msg_attr_aend,                   // pointer to allocated buffer that is used to holf attributes structures
		header_size,                       // buffer that has been allocated
		__builtin_addressof(real_buf_size) // the size that would be needed (in case of the buffer allocated was too small)
		));

	return p_msg_attr_aend;
}

template <nt_family family>
inline void *nt_family_ipc_alpc_client_connect_impl(nt_alpc_char_type const *server_name, ::std::size_t server_name_size, ::fast_io::ipc_mode mode,
													::std::byte const *message_begin, ::std::byte const *message_end, ::fast_io::win32::nt::alpc_message_attributes *__restrict message_attribute,
													nt_alpc_byte_vector &connect_recv_message)
{
	constexpr bool zw{family == nt_family::zw};

	if (::fast_io::details::is_invalid_dos_filename_with_size(server_name, server_name_size)) [[unlikely]]
	{
		throw_nt_error(3221225524);
	}

	auto temp_ipc_name_tlc_str{concat_nt_alpc_internal_tlc_str(u"\\RPC Control\\fast_io_ipc_", ::fast_io::mnp::os_c_str_with_known_size(server_name, server_name_size))};

	::fast_io::win32::nt::unicode_string us{};
	us.Buffer = const_cast<char16_t *>(temp_ipc_name_tlc_str.c_str());
	auto const temp_ipc_name_tlc_str_size_bytes{temp_ipc_name_tlc_str.size_bytes()};
	us.Length = static_cast<::std::uint_least16_t>(temp_ipc_name_tlc_str_size_bytes);
	us.MaximumLength = ::fast_io::win32::nt::details::nt_filename_bytes_check(temp_ipc_name_tlc_str_size_bytes + sizeof(char16_t));

	::fast_io::win32::nt::security_quality_of_service SecurityQos{};
	SecurityQos.ImpersonationLevel = ::fast_io::win32::nt::security_impersonation_level::SecurityImpersonation;
	SecurityQos.ContextTrackingMode = 0 /*SECURITY_STATIC_TRACKING*/;
	SecurityQos.EffectiveOnly = 0;
	SecurityQos.Length = sizeof(SecurityQos);

	::fast_io::win32::nt::alpc_port_attributes apa{};
	apa.Flags = 0x80000 /*ALPC_PORTFLG_ALLOW_DUP_OBJECT*/ |
				0x20000 /*ALPC_PORTFLG_ALLOW_LPC_REQUESTS*/;
	apa.MaxMessageLength = ::fast_io::win32::nt::AlpcMaxAllowedMessageLength();
	apa.SecurityQos = SecurityQos;

	::std::size_t const message_size{message_begin ? static_cast<::std::size_t>(message_end - message_begin) : 0u};
	::std::size_t const receive_size{sizeof(::fast_io::win32::nt::port_message) + message_size};

	/*
	 * The read data in the buffer is sent to the server and then synchronously waits for the return.
	 * The server accepts to return and then writes back to the same buffer.
	 * If the client buffer is insufficient, you can't even get the server handle.
	 * The server will not have any exceptions and cannot be connected again.
	 * This is Microsoft's wrong design.
	 * The maximum buffer (usually 64k) is used here to avoid this problem.
	 */

	::std::size_t const nt_alpc_max_message_length{static_cast<::std::size_t>(apa.MaxMessageLength)};

	::std::size_t connection_message_size{nt_alpc_max_message_length};

	if (receive_size > nt_alpc_max_message_length) [[unlikely]]
	{
		throw_nt_error(0xc0000701);
	}

	auto tmp{static_cast<::fast_io::win32::nt::alpc_message *>(nt_ipc_alpc_thread_local_heap_allocate_guard::alloc::allocate(nt_alpc_max_message_length))};
	nt_ipc_alpc_thread_local_heap_allocate_guard tmp_guard{tmp};

	::fast_io::freestanding::my_memset(tmp, 0, sizeof(::fast_io::win32::nt::port_message));

	// message data size
	tmp->PortHeader.u1.s1.DataLength = static_cast<::std::uint_least16_t>(message_size);
	// message total size
	tmp->PortHeader.u1.s1.TotalLength = static_cast<::std::uint_least16_t>(receive_size);

	if (message_begin)
	{
		::fast_io::freestanding::my_memcpy(tmp->PortMessage, message_begin, message_size);
	}

	using port_message_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= ::fast_io::win32::nt::port_message *;

	auto port_message_p{reinterpret_cast<port_message_may_alias_ptr>(tmp)};

	void *srv_common_port;

	check_nt_status(::fast_io::win32::nt::nt_alpc_connect_port<zw>(
		__builtin_addressof(srv_common_port), // REQUIRED: empty Communication port handle, fill be set by kernel
		__builtin_addressof(us),              // REQUIRED: Server Connect port name to connect to
		nullptr,                              // OPTIONAL: Object Attributes, none in this case
		__builtin_addressof(apa),             // OPTIONAL: PortAtrributes, used to set various port connection attributes, most imporatnly port flags
		(mode & ::fast_io::ipc_mode::sync) == ::fast_io::ipc_mode::sync ? 0x20000u /*ALPC_SYNC_CONNECTION*/ : 0u,
		nullptr,                                      // OPTIONAL: Server SID
		port_message_p,                               // connection message
		__builtin_addressof(connection_message_size), // connection message size
		nullptr,                                      // pMsgAttrSend,		// out messages attribtus
		message_attribute,                            // in message attributes
		nullptr                                       //&timeout				// OPTIONAL: Timeout, none in this case
		));

	if ((mode & ::fast_io::ipc_mode::sync) != ::fast_io::ipc_mode::sync)
	{
		connection_message_size = nt_alpc_max_message_length;
		check_nt_status(::fast_io::win32::nt::nt_alpc_send_wait_receive_port<zw>(
			srv_common_port, 0u, nullptr, nullptr, port_message_p,
			__builtin_addressof(connection_message_size), message_attribute, nullptr));
		if ((port_message_p->u2.s2.Type & 0xffu) != 11u) [[unlikely]]
		{
			::fast_io::win32::nt::nt_alpc_disconnect_port<zw>(srv_common_port, 0u);
			::fast_io::win32::nt::nt_close<zw>(srv_common_port);
			throw_nt_error(0xc0000701u);
		}
	}

	auto const recv_message_sizes{static_cast<::std::size_t>(port_message_p->u1.s1.DataLength)};
	connect_recv_message.clear();
	connect_recv_message.resize(recv_message_sizes);
	if (recv_message_sizes)
	{
		::fast_io::freestanding::my_memcpy(connect_recv_message.data(), tmp->PortMessage, recv_message_sizes);
	}

	return srv_common_port;
}

template <nt_family family>
struct nt_family_connect_alpc_ipc_server_paramenter
{
	using family_char_type = char16_t;
	ipc_mode im{};
	::std::byte const *mb{};
	::std::byte const *me{};
	::fast_io::win32::nt::alpc_message_attributes *ma{};
	nt_alpc_byte_vector &bv;

	inline void *operator()(family_char_type const *filename, ::std::size_t filename_size)
	{
		return nt_family_ipc_alpc_client_connect_impl<family>(filename, filename_size, im, mb, me, ma, bv);
	}
};

template <nt_family family, typename T>
	requires(::fast_io::constructible_to_os_c_str<T>)
inline void *nt_connect_alpc_ipc_server_impl(T const &t, ipc_mode im, ::std::byte const *mb, ::std::byte const *me, ::fast_io::win32::nt::alpc_message_attributes *ma, nt_alpc_byte_vector &bv)
{
	return ::fast_io::nt_api_common(t, nt_family_connect_alpc_ipc_server_paramenter<family>{im, mb, me, ma, bv});
}

inline constexpr ::std::uint_least32_t nt_status_timeout{0x00000102u};
inline constexpr ::std::uint_least32_t nt_status_unsuccessful{0xc0000001u};
inline constexpr ::std::uint_least32_t nt_status_buffer_too_small{0xc0000023u};
inline constexpr ::std::uint_least32_t nt_status_invalid_buffer_size{0xc0000206u};

template <nt_family family>
inline bool nt_alpc_receive_message_impl(void *__restrict port_handle,
										 ::fast_io::win32::nt::alpc_message_attributes *,
										 ::fast_io::nt_alpc_ipc_message &message, ::std::int_least64_t *timeout)
{
	constexpr bool zw{family == nt_family::zw};
	constexpr ::std::uint_least32_t context_attribute{0x20000000u /*ALPC_MESSAGE_CONTEXT_ATTRIBUTE*/};
	auto const maximum_message_size{static_cast<::std::size_t>(::fast_io::win32::nt::AlpcMaxAllowedMessageLength())};
	if (maximum_message_size < sizeof(::fast_io::win32::nt::port_message)) [[unlikely]]
	{
		throw_nt_error(nt_status_invalid_buffer_size);
	}

	auto storage{static_cast<::fast_io::win32::nt::alpc_message *>(nt_ipc_alpc_thread_local_heap_allocate_guard::alloc::allocate(maximum_message_size))};
	nt_ipc_alpc_thread_local_heap_allocate_guard storage_guard{storage};
	auto const attribute_size{::fast_io::win32::nt::AlpcGetHeaderSize(context_attribute)};
	if (!attribute_size) [[unlikely]]
	{
		throw_nt_error(nt_status_invalid_buffer_size);
	}
	auto receive_attributes{static_cast<::fast_io::win32::nt::alpc_message_attributes *>(
		nt_alpc_message_attribute_guard::alpc_message_alloc::allocate(attribute_size))};
	nt_alpc_message_attribute_guard attributes_guard{receive_attributes};
	::std::size_t required_attribute_size{};
	check_nt_status(::fast_io::win32::nt::AlpcInitializeMessageAttribute(
		context_attribute, receive_attributes, attribute_size, __builtin_addressof(required_attribute_size)));
	auto port_message_p{__builtin_addressof(storage->PortHeader)};
	::std::size_t receive_size{maximum_message_size};
	auto const status{::fast_io::win32::nt::nt_alpc_send_wait_receive_port<zw>(
		port_handle, 0u, nullptr, nullptr, port_message_p, __builtin_addressof(receive_size), receive_attributes, timeout)};
	if (status == nt_status_timeout ||
		(status == nt_status_unsuccessful && timeout && *timeout == 0))
	{
		return false;
	}
	if (status) [[unlikely]]
	{
		throw_nt_error(status);
	}

	auto const total_length{static_cast<::std::size_t>(port_message_p->u1.s1.TotalLength)};
	auto const data_length{static_cast<::std::size_t>(port_message_p->u1.s1.DataLength)};
	if (total_length < sizeof(::fast_io::win32::nt::port_message) || total_length > receive_size ||
		data_length > total_length - sizeof(::fast_io::win32::nt::port_message)) [[unlikely]]
	{
		throw_nt_error(nt_status_invalid_buffer_size);
	}

	message.bytes.clear();
	message.bytes.resize(data_length);
	if (data_length)
	{
		::fast_io::freestanding::my_memcpy(message.bytes.data(), storage->PortMessage, data_length);
	}
	message.sender = port_message_p->ClientId;
	auto context{static_cast<::fast_io::win32::nt::alpc_context_attr *>(
		::fast_io::win32::nt::AlpcGetMessageAttribute(receive_attributes, context_attribute))};
	message.port_context = context ? context->PortContext : nullptr;
	message.message_id = port_message_p->MessageId;
	message.callback_id = port_message_p->CallbackId;
	message.type = port_message_p->u2.s2.Type;
	return true;
}

template <nt_family family>
inline void nt_alpc_send_message_impl(void *__restrict port_handle, ::std::uint_least32_t flags,
									  ::std::uint_least32_t reply_message_id, ::std::byte const *first, ::std::byte const *last,
									  ::fast_io::win32::nt::alpc_message_attributes *__restrict send_attributes,
									  ::fast_io::nt_alpc_ipc_message *response)
{
	constexpr bool zw{family == nt_family::zw};
	auto const message_data_size{first ? static_cast<::std::size_t>(last - first) : 0u};
	auto const send_size{sizeof(::fast_io::win32::nt::port_message) + message_data_size};
	auto const maximum_message_size{static_cast<::std::size_t>(::fast_io::win32::nt::AlpcMaxAllowedMessageLength())};
	if (send_size > maximum_message_size || send_size > static_cast<::std::size_t>(::std::numeric_limits<::std::uint_least16_t>::max())) [[unlikely]]
	{
		throw_nt_error(nt_status_invalid_buffer_size);
	}

	auto send_storage{static_cast<::fast_io::win32::nt::alpc_message *>(nt_ipc_alpc_thread_local_heap_allocate_guard::alloc::allocate(send_size))};
	nt_ipc_alpc_thread_local_heap_allocate_guard send_storage_guard{send_storage};
	::fast_io::freestanding::my_memset(send_storage, 0, sizeof(::fast_io::win32::nt::port_message));
	if (message_data_size)
	{
		::fast_io::freestanding::my_memcpy(send_storage->PortMessage, first, message_data_size);
	}
	auto send_port_message{__builtin_addressof(send_storage->PortHeader)};
	send_port_message->u1.s1.DataLength = static_cast<::std::uint_least16_t>(message_data_size);
	send_port_message->u1.s1.TotalLength = static_cast<::std::uint_least16_t>(send_size);
	send_port_message->MessageId = reply_message_id;

	if (!response)
	{
		check_nt_status(::fast_io::win32::nt::nt_alpc_send_wait_receive_port<zw>(
			port_handle, flags, send_port_message, send_attributes, nullptr, nullptr, nullptr, nullptr));
		return;
	}

	auto receive_storage{static_cast<::fast_io::win32::nt::alpc_message *>(nt_ipc_alpc_thread_local_heap_allocate_guard::alloc::allocate(maximum_message_size))};
	nt_ipc_alpc_thread_local_heap_allocate_guard receive_storage_guard{receive_storage};
	::std::size_t receive_size{maximum_message_size};
	auto receive_port_message{__builtin_addressof(receive_storage->PortHeader)};
	check_nt_status(::fast_io::win32::nt::nt_alpc_send_wait_receive_port<zw>(
		port_handle, flags, send_port_message, send_attributes, receive_port_message,
		__builtin_addressof(receive_size), nullptr, nullptr));

	auto const total_length{static_cast<::std::size_t>(receive_port_message->u1.s1.TotalLength)};
	auto const data_length{static_cast<::std::size_t>(receive_port_message->u1.s1.DataLength)};
	if (total_length < sizeof(::fast_io::win32::nt::port_message) || total_length > receive_size ||
		data_length > total_length - sizeof(::fast_io::win32::nt::port_message)) [[unlikely]]
	{
		throw_nt_error(nt_status_invalid_buffer_size);
	}
	response->bytes.clear();
	response->bytes.resize(data_length);
	if (data_length)
	{
		::fast_io::freestanding::my_memcpy(response->bytes.data(), receive_storage->PortMessage, data_length);
	}
	response->sender = receive_port_message->ClientId;
	response->message_id = receive_port_message->MessageId;
	response->callback_id = receive_port_message->CallbackId;
	response->type = receive_port_message->u2.s2.Type;
}

template <nt_family family>
inline ::std::byte *nt_alpc_read_or_pread_some_bytes_common_impl(void *__restrict port_handle, ::std::byte *first, ::std::byte *last, ::fast_io::win32::nt::alpc_message_attributes *ama)
{
	constexpr bool zw{family == nt_family::zw};

	if (!first) [[unlikely]]
	{
		return nullptr;
	}

	auto const message_data_size{static_cast<::std::size_t>(last - first)};
	::std::size_t receive_size{sizeof(::fast_io::win32::nt::port_message) + message_data_size};

	auto tmp{static_cast<::fast_io::win32::nt::alpc_message *>(nt_ipc_alpc_thread_local_heap_allocate_guard::alloc::allocate(receive_size))};
	nt_ipc_alpc_thread_local_heap_allocate_guard tmp_guard{tmp};

	using port_message_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= ::fast_io::win32::nt::port_message *;

	auto port_message_p{reinterpret_cast<port_message_may_alias_ptr>(tmp)};

	auto status{::fast_io::win32::nt::nt_alpc_send_wait_receive_port<zw>(
		port_handle,
		0x0 /*ALPC_MSGFLG_LPC_MODE*/,
		nullptr,                           // SendMessage
		nullptr,                           // SendMessageAttributes
		port_message_p,                    // ReceiveBuffer
		__builtin_addressof(receive_size), // BufferLength
		ama,                               // ReceiveMessageAttributes
		nullptr                            // no timeout
		)};

	if (status == 0xc0000023)
	{
		// overflow
		return first;
	}
	else if (status) [[unlikely]]
	{
		throw_nt_error(status);
	}
	else [[likely]]
	{
		auto const actual_receive_size{static_cast<::std::size_t>(port_message_p->u1.s1.DataLength)};

		::fast_io::freestanding::my_memcpy(first, tmp->PortMessage, actual_receive_size);

		return first + actual_receive_size;
	}
}

template <nt_family family>
inline ::std::byte const *nt_alpc_write_or_pwrite_some_bytes_common_impl(void *__restrict port_handle, ::std::byte const *first, ::std::byte const *last, ::fast_io::win32::nt::alpc_message_attributes *ama)
{
	nt_alpc_send_message_impl<family>(port_handle, 0u, 0u, first, last, ama, nullptr);
	return last;
}

struct nt_alpc_port_associate_completion_port
{
	void *completion_key{};
	void *completion_port{};
};

template <nt_family family>
inline void nt_alpc_associate_completion_port_impl(void *port_handle, void *completion_port, ::std::size_t completion_key)
{
#if defined(_M_IX86)
	using nt_alpc_set_information_function = ::std::uint_least32_t(__stdcall *)(
		void *, ::std::uint_least32_t, void *, ::std::uint_least32_t) noexcept;
#elif defined(__i386__)
	using nt_alpc_set_information_function = ::std::uint_least32_t(__attribute__((__stdcall__)) *)(
		void *, ::std::uint_least32_t, void *, ::std::uint_least32_t) noexcept;
#else
	using nt_alpc_set_information_function = ::std::uint_least32_t (*)(
		void *, ::std::uint_least32_t, void *, ::std::uint_least32_t) noexcept;
#endif
	static auto const set_information{::std::bit_cast<nt_alpc_set_information_function>(
		::fast_io::win32::GetProcAddress(::fast_io::win32::GetModuleHandleA("ntdll.dll"),
										 family == nt_family::zw ? "ZwAlpcSetInformation" : "NtAlpcSetInformation"))};
	if (!set_information) [[unlikely]]
	{
		throw_nt_error(0xc000007au);
	}
	nt_alpc_port_associate_completion_port association{
		reinterpret_cast<void *>(completion_key), completion_port};
	check_nt_status(set_information(port_handle,
									static_cast<::std::uint_least32_t>(::fast_io::win32::nt::alpc_port_information_class::AlpcAssociateCompletionPortInformation),
									__builtin_addressof(association), static_cast<::std::uint_least32_t>(sizeof(association))));
}
} // namespace win32::nt::details

template <nt_family family, ::std::integral ch_type>
class basic_nt_family_alpc_ipc_universal_observer
{
public:
	using native_handle_type = ::fast_io::win32::nt::details::nt_alpc_handle<family> *;
	using char_type = ch_type;
	using input_char_type = char_type;
	using output_char_type = char_type;
	native_handle_type handle{};
	inline constexpr native_handle_type native_handle() const noexcept
	{
		return handle;
	}
	inline explicit operator bool() const noexcept
	{
		return handle != nullptr && handle->port_handle != nullptr;
	}
	inline constexpr native_handle_type release() noexcept
	{
		auto temp{handle};
		handle = nullptr;
		return temp;
	}
};

template <nt_family family, ::std::integral ch_type>
inline ::std::byte *read_some_bytes_underflow_define(basic_nt_family_alpc_ipc_universal_observer<family, ch_type> wiob,
													 ::std::byte *first, ::std::byte *last)
{
	if (!wiob) [[unlikely]]
	{
		throw_nt_error(0xc0000008);
	}
	if (wiob.handle->server_side_communication_port) [[unlikely]]
	{
		throw_nt_error(0xc0000184u);
	}
	if ((wiob.handle->mode & ::fast_io::ipc_mode::in) != ::fast_io::ipc_mode::in) [[unlikely]]
	{
		throw_nt_error(0xc0000184u);
	}

	switch (wiob.handle->status)
	{
	case win32::nt::details::nt_alpc_status::none:
	{
		if (!first) [[unlikely]]
		{
			return nullptr;
		}
		if (!wiob.handle->byte_vector.empty())
		{
			auto const available{wiob.handle->byte_vector.size()};
			auto const requested{static_cast<::std::size_t>(last - first)};
			auto const copied{requested < available ? requested : available};
			::fast_io::freestanding::my_memcpy(first, wiob.handle->byte_vector.data(), copied);
			wiob.handle->byte_vector.erase(wiob.handle->byte_vector.cbegin(), wiob.handle->byte_vector.cbegin() + copied);
			return first + copied;
		}

		::fast_io::nt_alpc_ipc_message message;
		::std::int_least64_t timeout{};
		auto const timeout_ptr{(wiob.handle->mode & ::fast_io::ipc_mode::no_block) == ::fast_io::ipc_mode::no_block ? __builtin_addressof(timeout) : nullptr};
		if (!::fast_io::win32::nt::details::nt_alpc_receive_message_impl<family>(
				wiob.handle->port_handle, wiob.handle->message_attribute, message, timeout_ptr))
		{
			return first;
		}
		auto const requested{static_cast<::std::size_t>(last - first)};
		auto const message_size{message.bytes.size()};
		auto const copied{requested < message_size ? requested : message_size};
		if (copied)
		{
			::fast_io::freestanding::my_memcpy(first, message.bytes.data(), copied);
		}
		if (copied != message_size)
		{
			auto const remaining{message_size - copied};
			wiob.handle->byte_vector.resize(remaining);
			::fast_io::freestanding::my_memcpy(wiob.handle->byte_vector.data(), message.bytes.data() + copied, remaining);
		}
		return first + copied;
	}
	case win32::nt::details::nt_alpc_status::after_connect:
	case win32::nt::details::nt_alpc_status::after_wait_for_connect:
	{
		if (first) [[likely]]
		{
			auto const read_size{static_cast<::std::size_t>(last - first)};
			if (auto const bv_size{wiob.handle->byte_vector.size()}; read_size >= bv_size)
			{
				if (wiob.handle->status == win32::nt::details::nt_alpc_status::after_connect)
				{
					wiob.handle->status = win32::nt::details::nt_alpc_status::none;
				}

				auto const bv_begin{wiob.handle->byte_vector.begin()};
				::fast_io::freestanding::my_memcpy(first, bv_begin, bv_size);
				wiob.handle->byte_vector.clear();
				return first + bv_size;
			}
			else
			{
				auto const bv_begin{wiob.handle->byte_vector.begin()};
				::fast_io::freestanding::my_memcpy(first, bv_begin, read_size);
				wiob.handle->byte_vector.erase(bv_begin, bv_begin + read_size);

				return last;
			}
		}
		else
		{
			return nullptr;
		}
	}
	default:
	{
		throw_nt_error(0x0c0000701);
	}
	}
}

template <nt_family family, ::std::integral ch_type>
inline ::std::byte const *write_some_bytes_overflow_define(basic_nt_family_alpc_ipc_universal_observer<family, ch_type> wiob,
														   ::std::byte const *first, ::std::byte const *last)
{
	if (!wiob) [[unlikely]]
	{
		throw_nt_error(0xc0000008);
	}
	if ((wiob.handle->mode & ::fast_io::ipc_mode::out) != ::fast_io::ipc_mode::out) [[unlikely]]
	{
		throw_nt_error(0xc0000184u);
	}

	switch (wiob.handle->status)
	{
	case win32::nt::details::nt_alpc_status::none:
	{
		if (!wiob.handle->communication_port ||
			(wiob.handle->mode & ::fast_io::ipc_mode::sync) == ::fast_io::ipc_mode::sync) [[unlikely]]
		{
			throw_nt_error(0xc0000184u);
		}
		return ::fast_io::win32::nt::details::nt_alpc_write_or_pwrite_some_bytes_common_impl<family>(wiob.handle->port_handle, first, last, wiob.handle->message_attribute);
	}
	case win32::nt::details::nt_alpc_status::after_connect:
	{
		throw_nt_error(0xc0000184u);
	}
	case win32::nt::details::nt_alpc_status::after_wait_for_connect:
	{
		wiob.handle->connection_reply.clear();
		wiob.handle->connection_reply_ready = true;

		if (first) [[likely]]
		{
			auto const write_size{static_cast<::std::size_t>(last - first)};
			wiob.handle->connection_reply.resize(write_size);
			if (write_size)
			{
				::fast_io::freestanding::my_memcpy(wiob.handle->connection_reply.data(), first, write_size);
			}
			return last;
		}
		else
		{
			return nullptr;
		}
	}
	default:
	{
		throw_nt_error(0x0c0000701);
	}
	}
}

template <nt_family family, ::std::integral ch_type>
inline constexpr basic_nt_family_alpc_ipc_universal_observer<family, ch_type>
io_stream_ref_define(basic_nt_family_alpc_ipc_universal_observer<family, ch_type> other) noexcept
{
	return other;
}

template <nt_family family, ::std::integral ch_type>
inline constexpr basic_nt_family_alpc_ipc_universal_observer<family, char>
io_bytes_stream_ref_define(basic_nt_family_alpc_ipc_universal_observer<family, ch_type> other) noexcept
{
	return {other.handle};
}

/*
 * Endpoint roles:
 * - The server listener receives connection requests, client messages, and ALPC control messages.
 * - An accepted server-side connection sends messages to exactly one client and is also its stable PortContext.
 * - A client connection sends asynchronous requests and receives server messages.
 * NtAlpcSendWaitReceivePort queues normal sends without waiting for a reply. IOCP association below is optional and
 * only supplies readiness notifications; alpc_receive/alpc_try_receive still remove messages from the ALPC queue.
 */
template <nt_family family, ::std::integral ch_type>
inline void alpc_send(basic_nt_family_alpc_ipc_universal_observer<family, ch_type> port,
					  ::std::span<::std::byte const> bytes)
{
	if (!port) [[unlikely]]
	{
		throw_nt_error(0xc0000008u);
	}
	if (!port.handle->communication_port ||
		(port.handle->mode & ::fast_io::ipc_mode::out) != ::fast_io::ipc_mode::out ||
		(port.handle->mode & ::fast_io::ipc_mode::sync) == ::fast_io::ipc_mode::sync ||
		port.handle->status != ::fast_io::win32::nt::details::nt_alpc_status::none) [[unlikely]]
	{
		throw_nt_error(0xc0000184u);
	}
	auto const first{bytes.empty() ? nullptr : bytes.data()};
	::fast_io::win32::nt::details::nt_alpc_send_message_impl<family>(
		port.handle->port_handle, 0u, 0u, first, first ? first + bytes.size() : nullptr,
		port.handle->message_attribute, nullptr);
}

template <nt_family family, ::std::integral ch_type>
inline void alpc_receive(basic_nt_family_alpc_ipc_universal_observer<family, ch_type> port,
						 nt_alpc_ipc_message &message)
{
	if (!port) [[unlikely]]
	{
		throw_nt_error(0xc0000008u);
	}
	if (port.handle->server_side_communication_port ||
		(port.handle->mode & ::fast_io::ipc_mode::in) != ::fast_io::ipc_mode::in ||
		port.handle->status != ::fast_io::win32::nt::details::nt_alpc_status::none) [[unlikely]]
	{
		throw_nt_error(0xc0000184u);
	}
	::fast_io::win32::nt::details::nt_alpc_receive_message_impl<family>(
		port.handle->port_handle, port.handle->message_attribute, message, nullptr);
	return;
}

template <nt_family family, ::std::integral ch_type>
inline nt_alpc_ipc_message alpc_receive(basic_nt_family_alpc_ipc_universal_observer<family, ch_type> port)
{
	nt_alpc_ipc_message message;
	::fast_io::alpc_receive(port, message);
	return message;
}

template <nt_family family, ::std::integral ch_type>
inline bool alpc_try_receive(basic_nt_family_alpc_ipc_universal_observer<family, ch_type> port,
							 nt_alpc_ipc_message &message)
{
	if (!port) [[unlikely]]
	{
		throw_nt_error(0xc0000008u);
	}
	if (port.handle->server_side_communication_port ||
		(port.handle->mode & ::fast_io::ipc_mode::in) != ::fast_io::ipc_mode::in ||
		port.handle->status != ::fast_io::win32::nt::details::nt_alpc_status::none) [[unlikely]]
	{
		throw_nt_error(0xc0000184u);
	}
	::std::int_least64_t timeout{};
	return ::fast_io::win32::nt::details::nt_alpc_receive_message_impl<family>(
		port.handle->port_handle, port.handle->message_attribute, message, __builtin_addressof(timeout));
}

template <nt_family family, ::std::integral ch_type, typename Rep, typename Period>
inline bool alpc_receive_for(basic_nt_family_alpc_ipc_universal_observer<family, ch_type> port,
							 nt_alpc_ipc_message &message, ::std::chrono::duration<Rep, Period> duration)
{
	if (!port) [[unlikely]]
	{
		throw_nt_error(0xc0000008u);
	}
	if (port.handle->server_side_communication_port ||
		(port.handle->mode & ::fast_io::ipc_mode::in) != ::fast_io::ipc_mode::in ||
		port.handle->status != ::fast_io::win32::nt::details::nt_alpc_status::none) [[unlikely]]
	{
		throw_nt_error(0xc0000184u);
	}
	auto const nanoseconds{::std::chrono::duration_cast<::std::chrono::nanoseconds>(duration).count()};
	::std::int_least64_t timeout{};
	if (nanoseconds > 0)
	{
		auto ticks{nanoseconds / 100};
		if (nanoseconds % 100)
		{
			++ticks;
		}
		timeout = -ticks;
	}
	return ::fast_io::win32::nt::details::nt_alpc_receive_message_impl<family>(
		port.handle->port_handle, port.handle->message_attribute, message, __builtin_addressof(timeout));
}

template <nt_family family, ::std::integral ch_type>
inline void alpc_request(basic_nt_family_alpc_ipc_universal_observer<family, ch_type> port,
						 ::std::span<::std::byte const> bytes, nt_alpc_ipc_message &response)
{
	if (!port) [[unlikely]]
	{
		throw_nt_error(0xc0000008u);
	}
	if (!port.handle->communication_port || port.handle->server_side_communication_port ||
		(port.handle->mode & (::fast_io::ipc_mode::in | ::fast_io::ipc_mode::out)) !=
			(::fast_io::ipc_mode::in | ::fast_io::ipc_mode::out) ||
		(port.handle->mode & ::fast_io::ipc_mode::sync) != ::fast_io::ipc_mode::sync ||
		port.handle->status != ::fast_io::win32::nt::details::nt_alpc_status::none) [[unlikely]]
	{
		throw_nt_error(0xc0000184u);
	}
	auto const first{bytes.empty() ? nullptr : bytes.data()};
	::fast_io::win32::nt::details::nt_alpc_send_message_impl<family>(
		port.handle->port_handle, 0x00020000u /*ALPC_MSGFLG_SYNC_REQUEST*/, 0u,
		first, first ? first + bytes.size() : nullptr, port.handle->message_attribute,
		__builtin_addressof(response));
	return;
}

template <nt_family family, ::std::integral ch_type>
inline nt_alpc_ipc_message alpc_request(basic_nt_family_alpc_ipc_universal_observer<family, ch_type> port,
										::std::span<::std::byte const> bytes)
{
	nt_alpc_ipc_message response;
	::fast_io::alpc_request(port, bytes, response);
	return response;
}

template <nt_family family, ::std::integral ch_type>
inline void alpc_reply(basic_nt_family_alpc_ipc_universal_observer<family, ch_type> port,
					   nt_alpc_ipc_message const &request, ::std::span<::std::byte const> bytes)
{
	if (!port) [[unlikely]]
	{
		throw_nt_error(0xc0000008u);
	}
	if (port.handle->communication_port ||
		(port.handle->mode & ::fast_io::ipc_mode::out) != ::fast_io::ipc_mode::out ||
		(port.handle->mode & ::fast_io::ipc_mode::sync) != ::fast_io::ipc_mode::sync ||
		port.handle->status != ::fast_io::win32::nt::details::nt_alpc_status::none ||
		alpc_message_type(request) != nt_alpc_ipc_message_type::request) [[unlikely]]
	{
		throw_nt_error(0xc0000184u);
	}
	auto const first{bytes.empty() ? nullptr : bytes.data()};
	::fast_io::win32::nt::details::nt_alpc_send_message_impl<family>(
		port.handle->port_handle, 0x00000001u /*ALPC_MSGFLG_REPLY_MESSAGE*/, request.message_id,
		first, first ? first + bytes.size() : nullptr, port.handle->message_attribute, nullptr);
}

template <nt_family family, ::std::integral ch_type, typename scheduler_type>
	requires requires(scheduler_type const &scheduler) { scheduler.native_handle(); }
inline void alpc_associate_io_completion_port(
	basic_nt_family_alpc_ipc_universal_observer<family, ch_type> port,
	scheduler_type const &scheduler, ::std::size_t completion_key)
{
	if (!port || !scheduler.native_handle()) [[unlikely]]
	{
		throw_nt_error(0xc0000008u);
	}
	::fast_io::win32::nt::details::nt_alpc_associate_completion_port_impl<family>(
		port.handle->port_handle, scheduler.native_handle(), completion_key);
}

template <typename scheduler_type>
	requires requires(scheduler_type const &scheduler) { scheduler.native_handle(); }
inline nt_alpc_ipc_completion alpc_wait(scheduler_type const &scheduler)
{
	::std::uint_least32_t transferred{};
	::std::size_t completion_key{};
	::fast_io::win32::overlapped *overlapped{};
	if (!::fast_io::win32::GetQueuedCompletionStatus(scheduler.native_handle(), __builtin_addressof(transferred),
													 __builtin_addressof(completion_key), __builtin_addressof(overlapped),
													 ::std::numeric_limits<::std::uint_least32_t>::max())) [[unlikely]]
	{
		throw_win32_error();
	}
	return {completion_key, transferred, overlapped};
}

template <typename scheduler_type, typename Rep, typename Period>
	requires requires(scheduler_type const &scheduler) { scheduler.native_handle(); }
inline bool alpc_wait_for(scheduler_type const &scheduler, nt_alpc_ipc_completion &completion,
						  ::std::chrono::duration<Rep, Period> duration)
{
	auto milliseconds{::std::chrono::duration_cast<::std::chrono::milliseconds>(duration).count()};
	if (milliseconds < 0)
	{
		milliseconds = 0;
	}
	else if (milliseconds == 0 && duration > duration.zero())
	{
		milliseconds = 1;
	}
	auto const maximum_timeout{static_cast<decltype(milliseconds)>(
		::std::numeric_limits<::std::uint_least32_t>::max() - 1u)};
	if (milliseconds > maximum_timeout)
	{
		milliseconds = maximum_timeout;
	}
	::std::uint_least32_t transferred{};
	::std::size_t completion_key{};
	::fast_io::win32::overlapped *overlapped{};
	if (!::fast_io::win32::GetQueuedCompletionStatus(scheduler.native_handle(), __builtin_addressof(transferred),
													 __builtin_addressof(completion_key), __builtin_addressof(overlapped),
													 static_cast<::std::uint_least32_t>(milliseconds)))
	{
		auto const error{::fast_io::win32::GetLastError()};
		if (error == 258u)
		{
			return false;
		}
		throw_win32_error(error);
	}
	completion = {completion_key, transferred, overlapped};
	return true;
}

template <nt_family family, ::std::integral char_type>
inline constexpr ::std::true_type print_semantic_optional_scatter_plan_stream(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::basic_nt_family_alpc_ipc_universal_observer<
			family, char_type>>) noexcept
{
	// The exact ALPC observer has no associated whole-record status customization. It exposes no native scatter or
	// fallback-coalescing policy, so the consumer cannot combine independently framed messages. Any admitted bounded
	// segment therefore retains the ordinary write CPO, handle state, message attributes, and exception-visible prefix.
	return {};
}

template <nt_family family, ::std::integral char_type>
inline constexpr ::std::true_type
	print_semantic_optional_scatter_barrier_plan_stream(
		::fast_io::io_reserve_type_t<
			char_type,
			::fast_io::basic_nt_family_alpc_ipc_universal_observer<
				family, char_type>>) noexcept
{
	// Ordinary control dispatch completes the preceding prefix before a direct-only barrier. Segment dispatch uses the
	// same named observer and reaches the same ALPC status and byte-vector state afterward, preserving message calls,
	// failure prefixes, and line ownership without granting wrappers or transcoders this proof.
	return {};
}

template <nt_family family, ::std::integral ch_type>
using basic_nt_family_alpc_ipc_server_observer = basic_nt_family_alpc_ipc_universal_observer<family, ch_type>;

template <nt_family family, ::std::integral ch_type>
class basic_nt_family_alpc_ipc_server : public basic_nt_family_alpc_ipc_server_observer<family, ch_type>
{
public:
	using typename basic_nt_family_alpc_ipc_server_observer<family, ch_type>::char_type;
	using typename basic_nt_family_alpc_ipc_server_observer<family, ch_type>::input_char_type;
	using typename basic_nt_family_alpc_ipc_server_observer<family, ch_type>::output_char_type;
	using typename basic_nt_family_alpc_ipc_server_observer<family, ch_type>::native_handle_type;
	using basic_nt_family_alpc_ipc_server_observer<family, ch_type>::native_handle;

	inline explicit constexpr basic_nt_family_alpc_ipc_server() noexcept = default;

	inline constexpr basic_nt_family_alpc_ipc_server(basic_nt_family_alpc_ipc_server_observer<family, ch_type>) noexcept = delete;
	inline constexpr basic_nt_family_alpc_ipc_server &operator=(basic_nt_family_alpc_ipc_server_observer<family, ch_type>) noexcept = delete;

	inline basic_nt_family_alpc_ipc_server(basic_nt_family_alpc_ipc_server const &) = delete;
	inline basic_nt_family_alpc_ipc_server &operator=(basic_nt_family_alpc_ipc_server const &) = delete;

	inline basic_nt_family_alpc_ipc_server(basic_nt_family_alpc_ipc_server &&__restrict b) noexcept
		: basic_nt_family_alpc_ipc_server_observer<family, ch_type>{b.release()}
	{
	}
	inline basic_nt_family_alpc_ipc_server &operator=(basic_nt_family_alpc_ipc_server &&__restrict b) noexcept
	{
		if (__builtin_addressof(b) == this) [[unlikely]]
		{
			return *this;
		}
		if (this->handle)
		{
			::fast_io::win32::nt::details::nt_alpc_deallocate_handle<family>(this->handle);
		}
		this->handle = b.handle;
		b.handle = nullptr;
		return *this;
	}
	inline void reset(native_handle_type newhandle = {}) noexcept
	{
		if (this->handle == newhandle)
		{
			return;
		}
		if (this->handle)
		{
			::fast_io::win32::nt::details::nt_alpc_deallocate_handle<family>(this->handle);
		}
		this->handle = newhandle;
	}
	inline void close()
	{
		if (this->handle)
		{
			auto old_handle{this->handle};
			this->handle = nullptr;
			try
			{
				old_handle->close();
			}
			catch (...)
			{
				::fast_io::win32::nt::details::nt_alpc_deallocate_handle<family>(old_handle);
				throw;
			}
			::fast_io::win32::nt::details::nt_alpc_deallocate_handle<family>(old_handle);
		}
	}
	inline void malloc_handle()
	{
		if (!this->handle) [[likely]]
		{
			this->handle = ::fast_io::win32::nt::details::nt_alpc_allocate_handle<family>();
		}
	}

	template <typename native_hd>
		requires ::std::same_as<native_handle_type, ::std::remove_cvref_t<native_hd>>
	inline explicit constexpr basic_nt_family_alpc_ipc_server(native_hd handle1) noexcept
		: basic_nt_family_alpc_ipc_server_observer<family, ch_type>{handle1}
	{}

	template <::fast_io::constructible_to_os_c_str T>
	inline explicit basic_nt_family_alpc_ipc_server(T const &server_name, ipc_mode im)
	{
		this->handle = ::fast_io::win32::nt::details::nt_alpc_allocate_handle<family>();
		this->handle->mode = im;
		try
		{
			this->handle->port_handle = ::fast_io::win32::nt::details::nt_create_alpc_ipc_server_impl<family>(server_name, im);
		}
		catch (...)
		{
			::fast_io::win32::nt::details::nt_alpc_deallocate_handle<family>(this->handle);
			this->handle = nullptr;
			throw;
		}
	}

	inline ~basic_nt_family_alpc_ipc_server()
	{
		if (this->handle)
		{
			::fast_io::win32::nt::details::nt_alpc_deallocate_handle<family>(this->handle);
		}
	}
};

template <nt_family family, ::std::integral ch_type>
using basic_nt_family_alpc_ipc_client_observer = basic_nt_family_alpc_ipc_universal_observer<family, ch_type>;

template <nt_family family, ::std::integral ch_type>
class basic_nt_family_alpc_ipc_client : public basic_nt_family_alpc_ipc_client_observer<family, ch_type>
{
public:
	using typename basic_nt_family_alpc_ipc_client_observer<family, ch_type>::char_type;
	using typename basic_nt_family_alpc_ipc_client_observer<family, ch_type>::input_char_type;
	using typename basic_nt_family_alpc_ipc_client_observer<family, ch_type>::output_char_type;
	using typename basic_nt_family_alpc_ipc_client_observer<family, ch_type>::native_handle_type;
	using basic_nt_family_alpc_ipc_client_observer<family, ch_type>::native_handle;

	inline explicit constexpr basic_nt_family_alpc_ipc_client() noexcept = default;

	inline constexpr basic_nt_family_alpc_ipc_client(basic_nt_family_alpc_ipc_client_observer<family, ch_type>) noexcept = delete;
	inline constexpr basic_nt_family_alpc_ipc_client &operator=(basic_nt_family_alpc_ipc_client_observer<family, ch_type>) noexcept = delete;

	inline basic_nt_family_alpc_ipc_client(basic_nt_family_alpc_ipc_client const &) = delete;
	inline basic_nt_family_alpc_ipc_client &operator=(basic_nt_family_alpc_ipc_client const &) = delete;

	inline basic_nt_family_alpc_ipc_client(basic_nt_family_alpc_ipc_client &&__restrict b) noexcept
		: basic_nt_family_alpc_ipc_client_observer<family, ch_type>{b.release()}
	{
	}
	inline basic_nt_family_alpc_ipc_client &operator=(basic_nt_family_alpc_ipc_client &&__restrict b) noexcept
	{
		if (__builtin_addressof(b) == this) [[unlikely]]
		{
			return *this;
		}
		if (this->handle)
		{
			::fast_io::win32::nt::details::nt_alpc_deallocate_handle<family>(this->handle);
		}
		this->handle = b.handle;
		b.handle = nullptr;
		return *this;
	}
	inline void reset(native_handle_type newhandle = {}) noexcept
	{
		if (this->handle == newhandle)
		{
			return;
		}
		if (this->handle)
		{
			::fast_io::win32::nt::details::nt_alpc_deallocate_handle<family>(this->handle);
		}
		this->handle = newhandle;
	}
	inline void close()
	{
		if (this->handle)
		{
			auto old_handle{this->handle};
			this->handle = nullptr;
			try
			{
				old_handle->close();
			}
			catch (...)
			{
				::fast_io::win32::nt::details::nt_alpc_deallocate_handle<family>(old_handle);
				throw;
			}
			::fast_io::win32::nt::details::nt_alpc_deallocate_handle<family>(old_handle);
		}
	}
	inline void malloc_handle()
	{
		if (!this->handle) [[likely]]
		{
			this->handle = ::fast_io::win32::nt::details::nt_alpc_allocate_handle<family>();
		}
	}

	template <typename native_hd>
		requires ::std::same_as<native_handle_type, ::std::remove_cvref_t<native_hd>>
	inline explicit constexpr basic_nt_family_alpc_ipc_client(native_hd handle1) noexcept
		: basic_nt_family_alpc_ipc_client_observer<family, ch_type>{handle1}
	{}

	template <::fast_io::constructible_to_os_c_str T>
	inline explicit basic_nt_family_alpc_ipc_client(T const &client_name, ipc_mode im)
	{
		this->handle = ::fast_io::win32::nt::details::nt_alpc_allocate_handle<family>();
		this->handle->mode = im;
		try
		{
			this->handle->port_handle = ::fast_io::win32::nt::details::nt_connect_alpc_ipc_server_impl<family>(client_name, im, nullptr, nullptr, nullptr, this->handle->byte_vector);
			this->handle->status = this->handle->byte_vector.empty() ? ::fast_io::win32::nt::details::nt_alpc_status::none : ::fast_io::win32::nt::details::nt_alpc_status::after_connect;
			this->handle->communication_port = true;
		}
		catch (...)
		{
			::fast_io::win32::nt::details::nt_alpc_deallocate_handle<family>(this->handle);
			this->handle = nullptr;
			throw;
		}
	}

	template <::fast_io::constructible_to_os_c_str T>
	inline explicit basic_nt_family_alpc_ipc_client(T const &client_name, ipc_mode im, ::fast_io::win32::nt::details::nt_alpc_communication_tlc_strvw<ch_type> message)
	{
		auto const str_begin{reinterpret_cast<::std::byte const *>(message.data())};
		auto const str_size{message.size_bytes()};

		this->handle = ::fast_io::win32::nt::details::nt_alpc_allocate_handle<family>();
		this->handle->mode = im;
		try
		{
			this->handle->port_handle = ::fast_io::win32::nt::details::nt_connect_alpc_ipc_server_impl<family>(client_name, im, str_begin, str_begin + str_size, nullptr, this->handle->byte_vector);
			this->handle->status = this->handle->byte_vector.empty() ? ::fast_io::win32::nt::details::nt_alpc_status::none : ::fast_io::win32::nt::details::nt_alpc_status::after_connect;
			this->handle->communication_port = true;
		}
		catch (...)
		{
			::fast_io::win32::nt::details::nt_alpc_deallocate_handle<family>(this->handle);
			this->handle = nullptr;
			throw;
		}
	}

	inline ~basic_nt_family_alpc_ipc_client()
	{
		if (this->handle)
		{
			::fast_io::win32::nt::details::nt_alpc_deallocate_handle<family>(this->handle);
		}
	}
};

/*
 * NOTE: Alpc related local operations are not thread safe, please add mutex
 */

template <nt_family server_family, ::std::integral server_ch_type, nt_family client_family = nt_family::nt, ::std::integral client_ch_type = char>
inline basic_nt_family_alpc_ipc_client<client_family, client_ch_type> wait_for_connect(
	basic_nt_family_alpc_ipc_server_observer<server_family, server_ch_type> server)
{
	if (server) [[likely]]
	{
		auto cid{win32::nt::details::nt_family_alpc_ipc_server_wait_for_connect_and_write_bv_impl<server_family>(
			server.handle->port_handle, server.handle->message_attribute,
			server.handle->byte_vector)};

		server.handle->status = win32::nt::details::nt_alpc_status::after_wait_for_connect;
		server.handle->connection_reply.clear();
		server.handle->connection_reply_ready = false;

		basic_nt_family_alpc_ipc_client<client_family, client_ch_type> ret;
		ret.malloc_handle();
		ret.handle->cid = cid;
		ret.handle->mode = server.handle->mode;

		return ret;
	}
	else
	{
		throw_nt_error(0xc0000008);
	}
}

template <nt_family server_family, ::std::integral server_ch_type, nt_family client_family = nt_family::nt, ::std::integral client_ch_type = char>
inline void accept_connect(
	basic_nt_family_alpc_ipc_server_observer<server_family, server_ch_type> server,
	basic_nt_family_alpc_ipc_client_observer<client_family, client_ch_type> client,
	bool accept)
{
	if (server /*check handle and handle->port_handle*/ && client.handle) [[likely]]
	{
		auto const reply_begin{server.handle->connection_reply_ready && !server.handle->connection_reply.empty() ? server.handle->connection_reply.data() : nullptr};
		auto client_port_handle{win32::nt::details::nt_family_alpc_ipc_server_accept_connect_and_send_impl<server_family>(
			server.handle->port_handle, client.handle->cid, accept, server.handle->message_attribute,
			reply_begin, reply_begin ? reply_begin + server.handle->connection_reply.size() : nullptr,
			server.handle->mode, client.handle)};

		server.handle->status = win32::nt::details::nt_alpc_status::none;
		server.handle->byte_vector.clear();
		server.handle->connection_reply.clear();
		server.handle->connection_reply_ready = false;

		client.handle->port_handle = client_port_handle;
		client.handle->communication_port = accept && client_port_handle;
		client.handle->server_side_communication_port = accept && client_port_handle;
	}
	else
	{
		throw_nt_error(0xc0000008);
	}
}

template <nt_family server_family, ::std::integral server_ch_type,
		  nt_family client_family = nt_family::nt, ::std::integral client_ch_type = char>
inline basic_nt_family_alpc_ipc_client<client_family, client_ch_type> accept_connect(
	basic_nt_family_alpc_ipc_server_observer<server_family, server_ch_type> server,
	nt_alpc_ipc_message const &request, bool accept,
	::std::span<::std::byte const> response = {})
{
	if (!server || server.handle->communication_port ||
		server.handle->status != win32::nt::details::nt_alpc_status::none ||
		alpc_message_type(request) != nt_alpc_ipc_message_type::connection_request) [[unlikely]]
	{
		throw_nt_error(0xc0000184u);
	}

	basic_nt_family_alpc_ipc_client<client_family, client_ch_type> client;
	client.malloc_handle();
	client.handle->cid = {request.sender, request.message_id};
	client.handle->mode = server.handle->mode;
	auto const response_begin{response.empty() ? nullptr : response.data()};
	client.handle->port_handle =
		win32::nt::details::nt_family_alpc_ipc_server_accept_connect_and_send_impl<server_family>(
			server.handle->port_handle, client.handle->cid, accept, server.handle->message_attribute,
			response_begin, response_begin ? response_begin + response.size() : nullptr,
			server.handle->mode, client.handle);
	client.handle->communication_port = accept && client.handle->port_handle;
	client.handle->server_side_communication_port = accept && client.handle->port_handle;
	return client;
}

template <nt_family client_family, ::std::integral client_ch_type>
inline void disconnect(basic_nt_family_alpc_ipc_universal_observer<client_family, client_ch_type> client)
{
	if (client) [[likely]]
	{
		client.handle->close();
	}
	else
	{
		throw_nt_error(0xc0000008);
	}
}

namespace freestanding
{
template <nt_family fm>
struct is_trivially_copyable_or_relocatable<win32::nt::details::nt_alpc_handle<fm>>
{
	// Accepted ports use this object's address as ALPC PortContext, so relocating it would break message routing.
	inline static constexpr bool value = false;
};

template <nt_family fm>
struct is_zero_default_constructible<win32::nt::details::nt_alpc_handle<fm>>
{
	inline static constexpr bool value = false;
};

template <nt_family fm, ::std::integral ch_type>
struct is_zero_default_constructible<basic_nt_family_alpc_ipc_universal_observer<fm, ch_type>>
{
	inline static constexpr bool value = true;
};

template <nt_family fm, ::std::integral ch_type>
struct is_trivially_copyable_or_relocatable<basic_nt_family_alpc_ipc_server<fm, ch_type>>
{
	inline static constexpr bool value = true;
};

template <nt_family fm, ::std::integral ch_type>
struct is_zero_default_constructible<basic_nt_family_alpc_ipc_server<fm, ch_type>>
{
	inline static constexpr bool value = true;
};

template <nt_family fm, ::std::integral ch_type>
struct is_trivially_copyable_or_relocatable<basic_nt_family_alpc_ipc_client<fm, ch_type>>
{
	inline static constexpr bool value = true;
};

template <nt_family fm, ::std::integral ch_type>
struct is_zero_default_constructible<basic_nt_family_alpc_ipc_client<fm, ch_type>>
{
	inline static constexpr bool value = true;
};
} // namespace freestanding

} // namespace fast_io
