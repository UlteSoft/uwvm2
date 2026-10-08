#include <cstddef>
#include <cstdint>

#if defined(_WIN32) && !defined(_WIN32_WINDOWS) && !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__)
// The actual target SDK, rather than a second guessed struct, is the layout
// oracle. This include does not replace any production declaration or API.
#include <windows.h>
#include <algorithm>
#include <array>
#include <type_traits>
#ifdef UWVM_TEST_IMPORT_FAST_IO
import fast_io;
#else
#include <fast_io.h>
#include <fast_io_hosted/filesystem/nt_readonly_sync_nothrow.h>
#endif

#ifndef UWVM_TEST_IMPORT_FAST_IO
static_assert(sizeof(::fast_io::win32::by_handle_file_information) == sizeof(::BY_HANDLE_FILE_INFORMATION));
static_assert(alignof(::fast_io::win32::by_handle_file_information) == alignof(::BY_HANDLE_FILE_INFORMATION));
static_assert(offsetof(::fast_io::win32::by_handle_file_information, dwVolumeSerialNumber) == offsetof(::BY_HANDLE_FILE_INFORMATION, dwVolumeSerialNumber));
static_assert(offsetof(::fast_io::win32::by_handle_file_information, nFileIndexHigh) == offsetof(::BY_HANDLE_FILE_INFORMATION, nFileIndexHigh));
static_assert(offsetof(::fast_io::win32::by_handle_file_information, nFileSizeHigh) == offsetof(::BY_HANDLE_FILE_INFORMATION, nFileSizeHigh));
static_assert(sizeof(::fast_io::win32::nt::file_basic_information) == sizeof(::FILE_BASIC_INFO));
static_assert(alignof(::fast_io::win32::nt::file_basic_information) == alignof(::FILE_BASIC_INFO));
static_assert(offsetof(::fast_io::win32::nt::file_basic_information, ChangeTime) == offsetof(::FILE_BASIC_INFO, ChangeTime));
static_assert(offsetof(::fast_io::win32::nt::file_basic_information, FileAttributes) == offsetof(::FILE_BASIC_INFO, FileAttributes));
#endif
static_assert(!::std::is_copy_constructible_v<::fast_io::nt_readonly_sync_file>);
static_assert(!::std::is_constructible_v<::fast_io::nt_readonly_sync_file, void*>);
static_assert(::std::is_nothrow_move_constructible_v<::fast_io::nt_readonly_sync_file>);

namespace
{
bool report(bool condition, char const* name)
{
	if (!condition) { ::fast_io::print(::fast_io::err(), "failure: ", ::fast_io::mnp::os_c_str(name), "\n"); }
	return condition;
}
}

int main(int argc, char** argv)
{
	// The keeper supplies an immutable isolated 10-byte "sync-read\n" file
	// and a separate absent path. This fixture never creates/truncates a file.
	if (argc != 3) { return 77; }
	bool ok{true};
	::fast_io::nt_readonly_sync_file empty{};
	::std::array<char, 16> storage{};
	auto invalid_read{::fast_io::nt_readonly_sync_read_nothrow(empty, storage.data(), 1)};
	ok &= report(!invalid_read && invalid_read.error.domain == ::fast_io::nt_domain_value && invalid_read.error.code == 0xc0000008u,
		"invalid owner native status");
	auto const invalid_close{::fast_io::nt_readonly_sync_close_nothrow(empty)};
	ok &= report(!invalid_close && invalid_close.error.code == 0xc0000008u, "invalid owner checked close");
	auto const missing{::fast_io::nt_open_readonly_sync_nothrow(::fast_io::mnp::os_c_str(argv[2]))};
	ok &= report(!missing && missing.error.domain == ::fast_io::win32_domain_value && missing.error.code != 0, "missing path real error");
	auto opened{::fast_io::nt_open_readonly_sync_nothrow(::fast_io::mnp::os_c_str(argv[1]))};
	if (!report(static_cast<bool>(opened), "readonly synchronous factory")) { return 1; }
	auto const before{::fast_io::nt_readonly_sync_status_nothrow(opened.file)};
	ok &= report(before && before.value.type == ::fast_io::file_type::regular && before.value.size == 10, "regular metadata size");
	auto const zero{::fast_io::nt_readonly_sync_read_nothrow(opened.file, nullptr, 0)};
	ok &= report(zero && zero.transferred == 0, "valid zero read");
	auto const bad_buffer{::fast_io::nt_readonly_sync_read_nothrow(opened.file, nullptr, 1)};
	ok &= report(!bad_buffer && bad_buffer.error.domain == ::fast_io::win32_domain_value && bad_buffer.error.code == 87, "null buffer rejected");
	if constexpr (sizeof(::std::size_t) > 4)
	{
		auto const too_large{::fast_io::nt_readonly_sync_read_nothrow(opened.file, storage.data(), static_cast<::std::size_t>(0x100000000ull))};
		ok &= report(!too_large && too_large.error.code == 87, "native ULONG range checked before IO");
	}
	storage[0] = 'L'; storage[11] = 'R';
	::std::size_t total{};
	while (total < 10)
	{
		// total<10, so 1+total<=10 and remaining<=10 in this owned 16-byte array.
		// [safe L storage[1..10] R storage[12..15]] end
		//          ^^ data()+1+total after the scalar bound check
		auto const read{::fast_io::nt_readonly_sync_read_nothrow(opened.file, storage.data() + 1 + total, 10 - total)};
		if (!report(read && read.transferred != 0 && read.transferred <= 10 - total, "actual bounded native read")) { return 1; }
		total += read.transferred;
	}
	constexpr char expected[]{"sync-read\n"};
	ok &= report(::std::equal(expected, expected + 10, storage.data() + 1) && storage[0] == 'L' && storage[11] == 'R',
		"unaligned read payload and unchanged surrounding bytes");
	auto const eof{::fast_io::nt_readonly_sync_read_nothrow(opened.file, storage.data(), 1)};
	ok &= report(eof && eof.transferred == 0, "real EOF distinct from failure");
#ifndef UWVM_TEST_IMPORT_FAST_IO
	// The header variant additionally exercises the incompatible WRITE open
	// through the existing fast_io noexcept ABI provider. The pure importer
	// variant uses only public exported factory/status/read/close names.
	auto const sharing{::fast_io::nt_api_common(::fast_io::mnp::os_c_str(argv[1]), [](char16_t const* path) noexcept
	{
		auto const handle{::fast_io::win32::CreateFileW(path, 0x40000000u, 7u, nullptr, 3u, 0x80u, nullptr)};
		if (handle == reinterpret_cast<void*>(static_cast<::std::intptr_t>(-1)))
		{
			auto const error{::fast_io::win32::GetLastError()};
			return ::fast_io::error{::fast_io::win32_domain_value, error};
		}
		::fast_io::native_file accidental_writer{handle};
		auto const status{::fast_io::win32::nt::nt_close<false>(accidental_writer.release())};
		return ::fast_io::error{::fast_io::nt_domain_value, status};
	})};
	ok &= report(sharing.domain == ::fast_io::win32_domain_value && sharing.code == 32, "live reader denies write sharing");
#endif
	auto const after{::fast_io::nt_readonly_sync_status_nothrow(opened.file)};
	ok &= report(before && after && before.value.dev == after.value.dev && before.value.ino == after.value.ino &&
		before.value.size == after.value.size && before.value.mtim == after.value.mtim && before.value.ctim == after.value.ctim,
		"identity and change-time unchanged across read");
	auto const closed{::fast_io::nt_readonly_sync_close_nothrow(opened.file)};
	ok &= report(closed && !opened.file, "file and private event owner retired once");
	auto const after_close{::fast_io::nt_readonly_sync_read_nothrow(opened.file, storage.data(), 1)};
	ok &= report(!after_close && after_close.error.code == 0xc0000008u, "retired owner rejects read");
	::fast_io::print("nt_readonly_sync_nothrow: ", ok ? "pass\n" : "fail\n");
	return ok ? 0 : 1;
}
#else
int main() { return 77; }
#endif
