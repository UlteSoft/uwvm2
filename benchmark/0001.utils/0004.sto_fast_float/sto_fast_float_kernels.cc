#include <fast_io_core.h>
#include <fast_float.h>

#include <cstdint>
#include <system_error>

namespace
{

inline void escape(void const* p) noexcept
{
	asm volatile("" : : "g"(p) : "memory");
}

inline std::uint64_t finish(std::uint64_t value, char const* ptr, char const* last, bool ok) noexcept
{
	return value + static_cast<std::uint64_t>(ptr == last) + (static_cast<std::uint64_t>(ok) << 1u);
}

template <int base, int length>
[[gnu::noinline]] std::uint64_t fast_float_kernel(char const* p) noexcept
{
	escape(p);
	std::uint64_t value{};
	auto const last{p + length};
	auto const ret{::fast_float::from_chars(p, last, value, base)};
	return finish(value, ret.ptr, last, ret.ec == std::errc{});
}

template <int base, int length>
[[gnu::noinline]] std::uint64_t fast_io_public_kernel(char const* p) noexcept
{
	escape(p);
	std::uint64_t value{};
	auto const last{p + length};
	auto const ret{::fast_io::parse_by_scan(p, last, ::fast_io::manipulators::base_get<base, true, true>(value))};
	return finish(value, ret.iter, last, ret.code == ::fast_io::parse_code::ok);
}

template <int base, int length>
[[gnu::noinline]] std::uint64_t fast_io_mask_kernel(char const* p) noexcept
{
	escape(p);
	std::uint64_t value{};
	auto const last{p + length};
	auto const ret{
		::fast_io::details::runtime_scan_int_contiguous_none_simd_space_part_define_impl<base, char, std::uint64_t>(
			p, last, value)};
	return finish(value, ret.iter, last, ret.code == ::fast_io::parse_code::ok);
}

template <int base, int length>
[[gnu::noinline]] std::uint64_t fast_io_scalar_kernel(char const* p) noexcept
{
	escape(p);
	std::uint64_t value{};
	auto const last{p + length};
	auto const ret{
		::fast_io::details::compile_time_scan_int_contiguous_none_simd_space_part_define_impl<base, char, std::uint64_t>(
			p, last, value)};
	return finish(value, ret.iter, last, ret.code == ::fast_io::parse_code::ok);
}

} // namespace

extern "C" std::uint64_t ff_10_8(char const* p) noexcept { return fast_float_kernel<10, 8>(p); }
extern "C" std::uint64_t fio_public_10_8(char const* p) noexcept { return fast_io_public_kernel<10, 8>(p); }
extern "C" std::uint64_t fio_mask_10_8(char const* p) noexcept { return fast_io_mask_kernel<10, 8>(p); }
extern "C" std::uint64_t fio_scalar_10_8(char const* p) noexcept { return fast_io_scalar_kernel<10, 8>(p); }

extern "C" std::uint64_t ff_10_16(char const* p) noexcept { return fast_float_kernel<10, 16>(p); }
extern "C" std::uint64_t fio_mask_10_16(char const* p) noexcept { return fast_io_mask_kernel<10, 16>(p); }
extern "C" std::uint64_t fio_scalar_10_16(char const* p) noexcept { return fast_io_scalar_kernel<10, 16>(p); }

extern "C" std::uint64_t ff_16_8(char const* p) noexcept { return fast_float_kernel<16, 8>(p); }
extern "C" std::uint64_t fio_mask_16_8(char const* p) noexcept { return fast_io_mask_kernel<16, 8>(p); }
extern "C" std::uint64_t fio_scalar_16_8(char const* p) noexcept { return fast_io_scalar_kernel<16, 8>(p); }

extern "C" std::uint64_t ff_16_15(char const* p) noexcept { return fast_float_kernel<16, 15>(p); }
extern "C" std::uint64_t fio_mask_16_15(char const* p) noexcept { return fast_io_mask_kernel<16, 15>(p); }
extern "C" std::uint64_t fio_scalar_16_15(char const* p) noexcept { return fast_io_scalar_kernel<16, 15>(p); }

extern "C" std::uint64_t ff_16_16(char const* p) noexcept { return fast_float_kernel<16, 16>(p); }
extern "C" std::uint64_t fio_mask_16_16(char const* p) noexcept { return fast_io_mask_kernel<16, 16>(p); }
extern "C" std::uint64_t fio_scalar_16_16(char const* p) noexcept { return fast_io_scalar_kernel<16, 16>(p); }
