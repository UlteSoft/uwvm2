#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include "posix_readlink_test_abi.h"
#ifdef UWVM_TEST_FAST_IO_MODULE
import fast_io;
#else
#include <fast_io.h>
#endif

namespace
{
::std::size_t checks{};
::std::size_t failures{};
void check(bool value, char const* label)
{
	++checks;
	if (!value) { ++failures; ::fast_io::io::perrln("readlink: ", label); }
}
struct guarded_bytes
{
	::std::array<char, 1026u> bytes;
	guarded_bytes() noexcept { bytes.fill(static_cast<char>(0xa5u)); }
	char* buffer() noexcept
	{
		// [one guard | 1024 caller-owned writable bytes | one guard] end
		// [safe] 1<1026 before moving to the beginning of the proved payload.
		return bytes.data() + 1u;
	}
	bool unchanged_tail(::std::size_t transferred) const noexcept
	{
		if (transferred > 1024u || bytes[0] != static_cast<char>(0xa5u)) return false;
		for (::std::size_t i{transferred + 1u}; i != bytes.size(); ++i)
		{
			// i<1026 before indexing; [written payload ...] [untouched suffix] end.
			if (bytes[i] != static_cast<char>(0xa5u)) return false;
		}
		return true;
	}
};
::fast_io::posix_read_result compare(int descriptor, char const* name, ::std::size_t count)
{
	guarded_bytes native, candidate;
	if (count > 1024u) { check(false, "fixture buffer extent"); return {0, EINVAL}; }
	auto const expected{readlink_sdk_oracle::invoke(descriptor, name, native.buffer(), count)};
	auto const actual{::fast_io::posix_readlinkat_nothrow(::fast_io::posix_at_entry{descriptor}, name, candidate.buffer(), count)};
	check(expected.error == actual.error && expected.transferred == actual.transferred, "native error/byte count");
	check(candidate.unchanged_tail(actual.transferred), "no terminator or out-of-extent write");
	check(native.unchanged_tail(expected.transferred), "SDK oracle extent");
	if (expected.error == 0 && actual.error == 0 && actual.transferred <= count)
	{
		// [1024 proved bytes] end; actual.transferred<=count<=1024 before end movement.
		check(::std::equal(candidate.buffer(), candidate.buffer() + actual.transferred, native.buffer()), "native payload bytes");
	}
	return actual;
}
}
int main(int argc, char** argv)
{
	// Native keeper creates an isolated directory containing a regular file and
	// a link; fixture itself only borrows names and opens a readonly directory.
	// argv[1..5] are complete NUL-terminated C runtime-owned strings after argc==6.
	if (argc != 6) return 2;
	auto directory{::fast_io::posix_openat_nothrow(::fast_io::posix_at_fdcwd(), argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC)};
	if (!directory) { ::fast_io::io::perrln("readlink directory open error=", ::fast_io::mnp::dec(directory.error)); return 1; }
	auto const fd{directory.file.native_handle()};
	auto const regular{compare(fd, argv[2], 512u)};
	check(regular.error == EINVAL, "regular file returns EINVAL without terminating noEH process");
	auto const expected_size{::fast_io::cstr_len(argv[4])};
	if (expected_size == 0 || expected_size > 512u) return 2;
	auto const link{compare(fd, argv[3], 512u)};
	check(link.error == 0 && link.transferred == expected_size, "link full payload length");
	guarded_bytes target;
	auto const actual_target{::fast_io::posix_readlinkat_nothrow(::fast_io::posix_at_entry{fd}, argv[3], target.buffer(), 512u)};
	if (actual_target.error == 0 && actual_target.transferred == expected_size)
	{
		// argv[4] has expected_size proved characters before NUL. Move only after
		// length equality and expected_size<=512<=1024 establish both read extents.
		check(::std::equal(argv[4], argv[4] + expected_size, target.buffer()), "expected symbolic target");
	}
	else check(false, "read target");
	check(target.unchanged_tail(actual_target.transferred), "full target output bound");
	auto const truncated{compare(fd, argv[3], 1u)};
	check(truncated.error == 0 && truncated.transferred == 1u, "one-byte truncation is a native success");
	(void)compare(fd, argv[3], 0u);
	(void)compare(fd, argv[2], 0u);
	auto const missing{compare(fd, argv[5], 512u)};
	check(missing.error == ENOENT, "missing relative name");
	(void)compare(-1, argv[3], 0u);
	auto const invalid{compare(-1, argv[3], 512u)};
	check(invalid.error == EBADF, "bad relative directory descriptor");
	auto const closed{::fast_io::posix_close_nothrow(directory.file)};
	check(closed.error == 0 && directory.file.native_handle() == -1, "owner closes exactly once");
	auto const retired{compare(fd, argv[3], 512u)};
	check(retired.error == EBADF, "actual closed directory descriptor");
	::fast_io::io::println("readlink checks=", ::fast_io::mnp::dec(checks), " failures=", ::fast_io::mnp::dec(failures));
	return failures == 0 ? 0 : 1;
}
