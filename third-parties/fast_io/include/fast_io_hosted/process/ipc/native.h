#pragma once

#if !defined(__MSDOS__) && !defined(__NEWLIB__) && !defined(__wasi__) && !defined(_PICOLIBC__)

#include "mode.h"

#if (defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__)
#include "win32/named_pipe_win32.h"
#if !defined(_WIN32_WINDOWS)
#include "win32/named_pipe_nt.h"
#if !defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x600
#include "win32/alpc_nt.h"
#endif
#endif
#elif !defined(__CYGWIN__) && __has_include(<sys/socket.h>) && __has_include(<sys/un.h>)
#include "posix/named_pipe.h"
#if defined(__linux__) || __has_include(<sys/eventfd.h>)
#include "posix/eventfd.h"
#endif
#endif

namespace fast_io
{

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__)

#if !defined(_WIN32_WINDOWS)
template <::std::integral ch_type>
using basic_native_named_pipe_ipc_server_observer = basic_nt_named_pipe_ipc_server_observer<ch_type>;
template <::std::integral ch_type>
using basic_native_named_pipe_ipc_server = basic_nt_named_pipe_ipc_server<ch_type>;
template <::std::integral ch_type>
using basic_native_named_pipe_ipc_client_observer = basic_nt_named_pipe_ipc_client_observer<ch_type>;
template <::std::integral ch_type>
using basic_native_named_pipe_ipc_client = basic_nt_named_pipe_ipc_client<ch_type>;
#else
template <::std::integral ch_type>
using basic_native_named_pipe_ipc_server_observer = basic_win32_named_pipe_ipc_server_observer<ch_type>;
template <::std::integral ch_type>
using basic_native_named_pipe_ipc_server = basic_win32_named_pipe_ipc_server<ch_type>;
template <::std::integral ch_type>
using basic_native_named_pipe_ipc_client_observer = basic_win32_named_pipe_ipc_client_observer<ch_type>;
template <::std::integral ch_type>
using basic_native_named_pipe_ipc_client = basic_win32_named_pipe_ipc_client<ch_type>;
#endif

#elif !defined(__CYGWIN__) && __has_include(<sys/socket.h>) && __has_include(<sys/un.h>)

template <::std::integral ch_type>
using basic_native_named_pipe_ipc_server_observer = basic_posix_named_pipe_ipc_server_observer<ch_type>;
template <::std::integral ch_type>
using basic_native_named_pipe_ipc_server = basic_posix_named_pipe_ipc_server<ch_type>;
template <::std::integral ch_type>
using basic_native_named_pipe_ipc_client_observer = basic_posix_named_pipe_ipc_client_observer<ch_type>;
template <::std::integral ch_type>
using basic_native_named_pipe_ipc_client = basic_posix_named_pipe_ipc_client<ch_type>;

#endif

#if (defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__)) || \
	(!defined(__CYGWIN__) && __has_include(<sys/socket.h>) && __has_include(<sys/un.h>))

using native_named_pipe_ipc_server_observer = basic_native_named_pipe_ipc_server_observer<char>;
using wnative_named_pipe_ipc_server_observer = basic_native_named_pipe_ipc_server_observer<wchar_t>;
using u8native_named_pipe_ipc_server_observer = basic_native_named_pipe_ipc_server_observer<char8_t>;
using u16native_named_pipe_ipc_server_observer = basic_native_named_pipe_ipc_server_observer<char16_t>;
using u32native_named_pipe_ipc_server_observer = basic_native_named_pipe_ipc_server_observer<char32_t>;

using native_named_pipe_ipc_server = basic_native_named_pipe_ipc_server<char>;
using wnative_named_pipe_ipc_server = basic_native_named_pipe_ipc_server<wchar_t>;
using u8native_named_pipe_ipc_server = basic_native_named_pipe_ipc_server<char8_t>;
using u16native_named_pipe_ipc_server = basic_native_named_pipe_ipc_server<char16_t>;
using u32native_named_pipe_ipc_server = basic_native_named_pipe_ipc_server<char32_t>;

using native_named_pipe_ipc_client_observer = basic_native_named_pipe_ipc_client_observer<char>;
using wnative_named_pipe_ipc_client_observer = basic_native_named_pipe_ipc_client_observer<wchar_t>;
using u8native_named_pipe_ipc_client_observer = basic_native_named_pipe_ipc_client_observer<char8_t>;
using u16native_named_pipe_ipc_client_observer = basic_native_named_pipe_ipc_client_observer<char16_t>;
using u32native_named_pipe_ipc_client_observer = basic_native_named_pipe_ipc_client_observer<char32_t>;

using native_named_pipe_ipc_client = basic_native_named_pipe_ipc_client<char>;
using wnative_named_pipe_ipc_client = basic_native_named_pipe_ipc_client<wchar_t>;
using u8native_named_pipe_ipc_client = basic_native_named_pipe_ipc_client<char8_t>;
using u16native_named_pipe_ipc_client = basic_native_named_pipe_ipc_client<char16_t>;
using u32native_named_pipe_ipc_client = basic_native_named_pipe_ipc_client<char32_t>;

#endif

#if !((defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__)) && __has_include(<sys/socket.h>) &&         \
	__has_include(<sys/un.h>) && (defined(__linux__) || __has_include(<sys/eventfd.h>))

template <::std::integral ch_type>
using basic_native_eventfd_observer = basic_posix_eventfd_observer<ch_type>;
template <::std::integral ch_type>
using basic_native_eventfd = basic_posix_eventfd<ch_type>;
using native_eventfd_observer = basic_native_eventfd_observer<char>;
using native_eventfd = basic_native_eventfd<char>;

#endif

} // namespace fast_io

#endif
