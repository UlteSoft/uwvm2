#include <fast_io_core.h>
#include <fast_float.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{

enum class algorithm : unsigned
{
	fast_float,
	fast_io_public,
	fast_io_mask,
	fast_io_scalar
};

enum class digit_case : unsigned
{
	normal,
	lower,
	upper,
	mixed
};

struct bench_options
{
	std::size_t samples{2048};
	std::size_t iters{16};
	std::size_t repeats{3};
};

struct input_case
{
	int base{};
	int length{};
	digit_case casing{};
	std::size_t extra_buffer_size{};
	std::vector<char> data;
};

struct bench_result
{
	double ns_per_parse{};
	std::uint64_t checksum{};
};

inline void escape(void const* p) noexcept
{
	asm volatile("" : : "g"(p) : "memory");
}

inline void clobber() noexcept
{
	asm volatile("" : : : "memory");
}

[[nodiscard]] constexpr char const* algorithm_name(algorithm alg) noexcept
{
	switch(alg)
	{
	case algorithm::fast_float: return "fast_float";
	case algorithm::fast_io_public: return "fast_io_public";
	case algorithm::fast_io_mask: return "fast_io_mask";
	case algorithm::fast_io_scalar: return "fast_io_scalar";
	}
	return "unknown";
}

[[nodiscard]] constexpr char const* case_name(digit_case dc) noexcept
{
	switch(dc)
	{
	case digit_case::normal: return "normal";
	case digit_case::lower: return "lower";
	case digit_case::upper: return "upper";
	case digit_case::mixed: return "mixed";
	}
	return "unknown";
}

[[nodiscard]] std::uint64_t splitmix64(std::uint64_t& state) noexcept
{
	std::uint64_t z{(state += 0x9e3779b97f4a7c15ull)};
	z = (z ^ (z >> 30u)) * 0xbf58476d1ce4e5b9ull;
	z = (z ^ (z >> 27u)) * 0x94d049bb133111ebull;
	return z ^ (z >> 31u);
}

[[nodiscard]] int max_digits_u64(int base) noexcept
{
	auto const limit{static_cast<unsigned __int128>((std::numeric_limits<std::uint64_t>::max)())};
	unsigned __int128 value{1};
	int digits{};
	for(;;)
	{
		++digits;
		if(value > limit / static_cast<unsigned>(base)) { break; }
		value *= static_cast<unsigned>(base);
	}
	return digits;
}

[[nodiscard]] char digit_to_char(int digit, int base, digit_case casing, int pos, std::uint64_t sample_index) noexcept
{
	(void)base;
	if(digit < 10) { return static_cast<char>('0' + digit); }
	if(casing == digit_case::upper) { return static_cast<char>('A' + digit - 10); }
	if(casing == digit_case::mixed)
	{
		bool upper{((sample_index + static_cast<std::uint64_t>(pos)) & 1u) != 0u};
		return static_cast<char>((upper ? 'A' : 'a') + digit - 10);
	}
	return static_cast<char>('a' + digit - 10);
}

[[nodiscard]] input_case make_input_case(int base, int length, digit_case casing, std::size_t extra_buffer_size,
										 std::size_t samples)
{
	input_case ic{.base = base, .length = length, .casing = casing, .extra_buffer_size = extra_buffer_size};
	auto const stride{static_cast<std::size_t>(length) + extra_buffer_size};
	ic.data.resize(samples * stride);

	std::uint64_t rng{0xd1b54a32d192ed03ull ^ (static_cast<std::uint64_t>(base) << 32u) ^
					  (static_cast<std::uint64_t>(length) << 8u) ^ static_cast<std::uint64_t>(casing)};
	auto const limit{static_cast<unsigned __int128>((std::numeric_limits<std::uint64_t>::max)())};
	std::string_view constexpr pad_chars{"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"};

	for(std::size_t sample{}; sample != samples; ++sample)
	{
		auto* out{ic.data.data() + sample * stride};
		for(;;)
		{
			unsigned __int128 value{};
			for(int pos{}; pos != length; ++pos)
			{
				int digit{};
				if(pos == 0)
				{
					digit = 1 + static_cast<int>(splitmix64(rng) % static_cast<std::uint64_t>(base - 1));
				}
				else
				{
					digit = static_cast<int>(splitmix64(rng) % static_cast<std::uint64_t>(base));
				}
				out[pos] = digit_to_char(digit, base, casing, pos, sample);
				value = value * static_cast<unsigned>(base) + static_cast<unsigned>(digit);
			}
			if(value <= limit) { break; }
		}
		for(std::size_t pad{}; pad != extra_buffer_size; ++pad)
		{
			out[static_cast<std::size_t>(length) + pad] =
				pad_chars[(sample + pad + static_cast<std::size_t>(base)) % pad_chars.size()];
		}
	}

	return ic;
}

template <int base>
[[gnu::noinline]] std::uint64_t run_fast_float(input_case const& ic, std::size_t iters) noexcept
{
	std::uint64_t checksum{};
	auto const* data{ic.data.data()};
	auto const length{static_cast<std::size_t>(ic.length)};
	auto const stride{length + ic.extra_buffer_size};
	auto const samples{ic.data.size() / stride};
	escape(data);
	for(std::size_t iter{}; iter != iters; ++iter)
	{
		for(std::size_t sample{}; sample != samples; ++sample)
		{
			auto const* first{data + sample * stride};
			auto const* last{first + length};
			std::uint64_t value{};
			auto const ret{::fast_float::from_chars(first, last, value, base)};
			checksum ^= value + 0x9e3779b97f4a7c15ull + (checksum << 6u) + (checksum >> 2u);
			checksum += static_cast<std::uint64_t>(ret.ptr == last);
			checksum += static_cast<std::uint64_t>(ret.ec == std::errc{}) << 1u;
		}
	}
	clobber();
	return checksum;
}

template <int base>
[[gnu::noinline]] std::uint64_t run_fast_io_public(input_case const& ic, std::size_t iters) noexcept
{
	std::uint64_t checksum{};
	auto const* data{ic.data.data()};
	auto const length{static_cast<std::size_t>(ic.length)};
	auto const stride{length + ic.extra_buffer_size};
	auto const samples{ic.data.size() / stride};
	escape(data);
	for(std::size_t iter{}; iter != iters; ++iter)
	{
		for(std::size_t sample{}; sample != samples; ++sample)
		{
			auto const* first{data + sample * stride};
			auto const* last{first + length};
			std::uint64_t value{};
			auto const ret{::fast_io::parse_by_scan(first, last, ::fast_io::manipulators::base_get<base, true, true>(value))};
			checksum ^= value + 0x9e3779b97f4a7c15ull + (checksum << 6u) + (checksum >> 2u);
			checksum += static_cast<std::uint64_t>(ret.iter == last);
			checksum += static_cast<std::uint64_t>(ret.code == ::fast_io::parse_code::ok) << 1u;
		}
	}
	clobber();
	return checksum;
}

template <int base>
[[gnu::noinline]] std::uint64_t run_fast_io_mask(input_case const& ic, std::size_t iters) noexcept
{
	std::uint64_t checksum{};
	auto const* data{ic.data.data()};
	auto const length{static_cast<std::size_t>(ic.length)};
	auto const stride{length + ic.extra_buffer_size};
	auto const samples{ic.data.size() / stride};
	escape(data);
	for(std::size_t iter{}; iter != iters; ++iter)
	{
		for(std::size_t sample{}; sample != samples; ++sample)
		{
			auto const* first{data + sample * stride};
			auto const* last{first + length};
			std::uint64_t value{};
			auto const ret{
				::fast_io::details::runtime_scan_int_contiguous_none_simd_space_part_define_impl<base, char, std::uint64_t>(
					first, last, value)};
			checksum ^= value + 0x9e3779b97f4a7c15ull + (checksum << 6u) + (checksum >> 2u);
			checksum += static_cast<std::uint64_t>(ret.iter == last);
			checksum += static_cast<std::uint64_t>(ret.code == ::fast_io::parse_code::ok) << 1u;
		}
	}
	clobber();
	return checksum;
}

template <int base>
[[gnu::noinline]] std::uint64_t run_fast_io_scalar(input_case const& ic, std::size_t iters) noexcept
{
	std::uint64_t checksum{};
	auto const* data{ic.data.data()};
	auto const length{static_cast<std::size_t>(ic.length)};
	auto const stride{length + ic.extra_buffer_size};
	auto const samples{ic.data.size() / stride};
	escape(data);
	for(std::size_t iter{}; iter != iters; ++iter)
	{
		for(std::size_t sample{}; sample != samples; ++sample)
		{
			auto const* first{data + sample * stride};
			auto const* last{first + length};
			std::uint64_t value{};
			auto const ret{
				::fast_io::details::compile_time_scan_int_contiguous_none_simd_space_part_define_impl<base, char, std::uint64_t>(
					first, last, value)};
			checksum ^= value + 0x9e3779b97f4a7c15ull + (checksum << 6u) + (checksum >> 2u);
			checksum += static_cast<std::uint64_t>(ret.iter == last);
			checksum += static_cast<std::uint64_t>(ret.code == ::fast_io::parse_code::ok) << 1u;
		}
	}
	clobber();
	return checksum;
}

template <int base>
[[nodiscard]] std::uint64_t run_one(algorithm alg, input_case const& ic, std::size_t iters) noexcept
{
	switch(alg)
	{
	case algorithm::fast_float: return run_fast_float<base>(ic, iters);
	case algorithm::fast_io_public: return run_fast_io_public<base>(ic, iters);
	case algorithm::fast_io_mask: return run_fast_io_mask<base>(ic, iters);
	case algorithm::fast_io_scalar: return run_fast_io_scalar<base>(ic, iters);
	}
	return {};
}

[[nodiscard]] std::uint64_t dispatch_run(int base, algorithm alg, input_case const& ic, std::size_t iters) noexcept
{
	switch(base)
	{
	case 2: return run_one<2>(alg, ic, iters);
	case 3: return run_one<3>(alg, ic, iters);
	case 4: return run_one<4>(alg, ic, iters);
	case 5: return run_one<5>(alg, ic, iters);
	case 6: return run_one<6>(alg, ic, iters);
	case 7: return run_one<7>(alg, ic, iters);
	case 8: return run_one<8>(alg, ic, iters);
	case 9: return run_one<9>(alg, ic, iters);
	case 10: return run_one<10>(alg, ic, iters);
	case 11: return run_one<11>(alg, ic, iters);
	case 12: return run_one<12>(alg, ic, iters);
	case 13: return run_one<13>(alg, ic, iters);
	case 14: return run_one<14>(alg, ic, iters);
	case 15: return run_one<15>(alg, ic, iters);
	case 16: return run_one<16>(alg, ic, iters);
	case 17: return run_one<17>(alg, ic, iters);
	case 18: return run_one<18>(alg, ic, iters);
	case 19: return run_one<19>(alg, ic, iters);
	case 20: return run_one<20>(alg, ic, iters);
	case 21: return run_one<21>(alg, ic, iters);
	case 22: return run_one<22>(alg, ic, iters);
	case 23: return run_one<23>(alg, ic, iters);
	case 24: return run_one<24>(alg, ic, iters);
	case 25: return run_one<25>(alg, ic, iters);
	case 26: return run_one<26>(alg, ic, iters);
	case 27: return run_one<27>(alg, ic, iters);
	case 28: return run_one<28>(alg, ic, iters);
	case 29: return run_one<29>(alg, ic, iters);
	case 30: return run_one<30>(alg, ic, iters);
	case 31: return run_one<31>(alg, ic, iters);
	case 32: return run_one<32>(alg, ic, iters);
	case 33: return run_one<33>(alg, ic, iters);
	case 34: return run_one<34>(alg, ic, iters);
	case 35: return run_one<35>(alg, ic, iters);
	case 36: return run_one<36>(alg, ic, iters);
	}
	return {};
}

[[nodiscard]] bench_result bench(input_case const& ic, algorithm alg, bench_options const& options)
{
	(void)dispatch_run(ic.base, alg, ic, 1);
	std::uint64_t best_ns{(std::numeric_limits<std::uint64_t>::max)()};
	std::uint64_t best_checksum{};

	for(std::size_t repeat{}; repeat != options.repeats; ++repeat)
	{
		auto const start{std::chrono::steady_clock::now()};
		auto const checksum{dispatch_run(ic.base, alg, ic, options.iters)};
		auto const stop{std::chrono::steady_clock::now()};
		auto const elapsed{static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count())};
		if(elapsed < best_ns)
		{
			best_ns = elapsed;
			best_checksum = checksum;
		}
	}

	auto const parses{static_cast<double>(options.iters) * static_cast<double>(options.samples)};
	return {.ns_per_parse = static_cast<double>(best_ns) / parses, .checksum = best_checksum};
}

[[nodiscard]] bool parse_size_arg(char const* arg, char const* prefix, std::size_t& out) noexcept
{
	auto const prefix_len{std::strlen(prefix)};
	if(std::strncmp(arg, prefix, prefix_len) != 0) { return false; }
	char* end{};
	errno = 0;
	auto const value{std::strtoull(arg + prefix_len, &end, 10)};
	if(errno || end == arg + prefix_len || *end != '\0' || value == 0) { return false; }
	out = static_cast<std::size_t>(value);
	return true;
}

[[nodiscard]] bench_options parse_options(int argc, char** argv)
{
	bench_options options;
	for(int i{1}; i != argc; ++i)
	{
		if(parse_size_arg(argv[i], "--samples=", options.samples)) { continue; }
		if(parse_size_arg(argv[i], "--iters=", options.iters)) { continue; }
		if(parse_size_arg(argv[i], "--repeats=", options.repeats)) { continue; }
		std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
		std::exit(2);
	}
	return options;
}

} // namespace

int main(int argc, char** argv)
{
	auto const options{parse_options(argc, argv)};
	std::array constexpr algorithms{algorithm::fast_float, algorithm::fast_io_public, algorithm::fast_io_mask,
									algorithm::fast_io_scalar};

	std::printf("base\tlength\tcase\textra_buffer\talgorithm\tns_per_parse\tchecksum\tsamples\titers\trepeats\n");
	for(int base{2}; base <= 36; ++base)
	{
		auto const max_digits{max_digits_u64(base)};
		for(int length{1}; length <= max_digits; ++length)
		{
			std::array<digit_case, 3> alpha_cases{digit_case::lower, digit_case::upper, digit_case::mixed};
			std::array<digit_case, 1> normal_cases{digit_case::normal};
			std::array<std::size_t, 2> extra_buffers{0u, 64u};
			auto const* cases{base >= 11 ? alpha_cases.data() : normal_cases.data()};
			auto const case_count{base >= 11 ? alpha_cases.size() : normal_cases.size()};
			for(auto extra_buffer_size: extra_buffers)
			{
				for(std::size_t ci{}; ci != case_count; ++ci)
				{
					auto ic{make_input_case(base, length, cases[ci], extra_buffer_size, options.samples)};
					for(auto alg: algorithms)
					{
						auto const result{bench(ic, alg, options)};
						std::printf("%d\t%d\t%s\t%zu\t%s\t%.6f\t%llu\t%zu\t%zu\t%zu\n", base, length,
									case_name(ic.casing), ic.extra_buffer_size, algorithm_name(alg),
									result.ns_per_parse, static_cast<unsigned long long>(result.checksum),
									options.samples, options.iters, options.repeats);
					}
				}
			}
		}
	}
}
