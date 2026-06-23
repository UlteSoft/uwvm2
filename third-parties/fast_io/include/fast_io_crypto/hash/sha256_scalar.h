#pragma once

inline constexpr auto create_k256scalar() noexcept
{
	constexpr ::std::size_t n{sizeof(K256) / sizeof(::std::uint_least32_t)};
	constexpr ::std::size_t nsub16{n - 16u};
	::fast_io::freestanding::array<::std::uint_least32_t, nsub16> a;
	for (::std::size_t i{}; i != nsub16; ++i)
	{
		a[i] = K256[i + 16u];
	}
	return a;
}

inline constexpr auto k256scalar{create_k256scalar()};

inline ::std::uint_least32_t sha256_load_be32_unaligned(::std::byte const *p) noexcept
{
	::std::uint_least32_t word{};
	::std::memcpy(__builtin_addressof(word), p, sizeof(word));
	return ::fast_io::big_endian(word);
}

#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
#if __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void sha256_round(::std::uint_least32_t T1, ::std::uint_least32_t a, ::std::uint_least32_t b,
								   ::std::uint_least32_t &__restrict d, ::std::uint_least32_t e,
								   ::std::uint_least32_t f, ::std::uint_least32_t g,
								   ::std::uint_least32_t &__restrict h, ::std::uint_least32_t &__restrict bpc,
								   ::std::uint_least32_t k) noexcept
{
	sha256_scalar_round(T1 + k, a, b, d, e, f, g, h, bpc);
}

#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
inline void sha256_runtime_routine(::std::uint_least32_t *__restrict state, ::std::byte const *__restrict blocks_start,
								   ::std::byte const *__restrict blocks_last) noexcept
{
	::std::uint_least32_t a{state[0]};
	::std::uint_least32_t b{state[1]};
	::std::uint_least32_t c{state[2]};
	::std::uint_least32_t d{state[3]};
	::std::uint_least32_t e{state[4]};
	::std::uint_least32_t f{state[5]};
	::std::uint_least32_t g{state[6]};
	::std::uint_least32_t h{state[7]};
	::std::uint_least32_t x[16];
	constexpr ::std::uint_least32_t const *k5_start{k256scalar.element};
	constexpr ::std::uint_least32_t const *k5_last{k256scalar.element + k256scalar.size()};
	for (; blocks_start != blocks_last; blocks_start += 64)
	{
		::std::uint_least32_t bpc{b ^ c};
		sha256_round(x[0] = sha256_load_be32_unaligned(blocks_start), a, b, d, e, f, g, h, bpc, 0x428A2F98);
		sha256_round(x[1] = sha256_load_be32_unaligned(blocks_start + 4), h, a, c, d, e, f, g, bpc, 0x71374491);
		sha256_round(x[2] = sha256_load_be32_unaligned(blocks_start + 8), g, h, b, c, d, e, f, bpc, 0xB5C0FBCF);
		sha256_round(x[3] = sha256_load_be32_unaligned(blocks_start + 12), f, g, a, b, c, d, e, bpc, 0xE9B5DBA5);
		sha256_round(x[4] = sha256_load_be32_unaligned(blocks_start + 16), e, f, h, a, b, c, d, bpc, 0x3956C25B);
		sha256_round(x[5] = sha256_load_be32_unaligned(blocks_start + 20), d, e, g, h, a, b, c, bpc, 0x59F111F1);
		sha256_round(x[6] = sha256_load_be32_unaligned(blocks_start + 24), c, d, f, g, h, a, b, bpc, 0x923F82A4);
		sha256_round(x[7] = sha256_load_be32_unaligned(blocks_start + 28), b, c, e, f, g, h, a, bpc, 0xAB1C5ED5);
		sha256_round(x[8] = sha256_load_be32_unaligned(blocks_start + 32), a, b, d, e, f, g, h, bpc, 0xD807AA98);
		sha256_round(x[9] = sha256_load_be32_unaligned(blocks_start + 36), h, a, c, d, e, f, g, bpc, 0x12835B01);
		sha256_round(x[10] = sha256_load_be32_unaligned(blocks_start + 40), g, h, b, c, d, e, f, bpc, 0x243185BE);
		sha256_round(x[11] = sha256_load_be32_unaligned(blocks_start + 44), f, g, a, b, c, d, e, bpc, 0x550C7DC3);
		sha256_round(x[12] = sha256_load_be32_unaligned(blocks_start + 48), e, f, h, a, b, c, d, bpc, 0x72BE5D74);
		sha256_round(x[13] = sha256_load_be32_unaligned(blocks_start + 52), d, e, g, h, a, b, c, bpc, 0x80DEB1FE);
		sha256_round(x[14] = sha256_load_be32_unaligned(blocks_start + 56), c, d, f, g, h, a, b, bpc, 0x9BDC06A7);
		sha256_round(x[15] = sha256_load_be32_unaligned(blocks_start + 60), b, c, e, f, g, h, a, bpc, 0xC19BF174);
		for (::std::uint_least32_t const *k5{k5_start}; k5 != k5_last; k5 += 16)
		{
			sha256_round((x[0] += sigma0(x[1]) + sigma1(x[14]) + x[9]), a, b, d, e, f, g, h, bpc, *k5);
			sha256_round((x[1] += sigma0(x[2]) + sigma1(x[15]) + x[10]), h, a, c, d, e, f, g, bpc, k5[1]);
			sha256_round((x[2] += sigma0(x[3]) + sigma1(x[0]) + x[11]), g, h, b, c, d, e, f, bpc, k5[2]);
			sha256_round((x[3] += sigma0(x[4]) + sigma1(x[1]) + x[12]), f, g, a, b, c, d, e, bpc, k5[3]);
			sha256_round((x[4] += sigma0(x[5]) + sigma1(x[2]) + x[13]), e, f, h, a, b, c, d, bpc, k5[4]);
			sha256_round((x[5] += sigma0(x[6]) + sigma1(x[3]) + x[14]), d, e, g, h, a, b, c, bpc, k5[5]);
			sha256_round((x[6] += sigma0(x[7]) + sigma1(x[4]) + x[15]), c, d, f, g, h, a, b, bpc, k5[6]);
			sha256_round((x[7] += sigma0(x[8]) + sigma1(x[5]) + x[0]), b, c, e, f, g, h, a, bpc, k5[7]);
			sha256_round((x[8] += sigma0(x[9]) + sigma1(x[6]) + x[1]), a, b, d, e, f, g, h, bpc, k5[8]);
			sha256_round((x[9] += sigma0(x[10]) + sigma1(x[7]) + x[2]), h, a, c, d, e, f, g, bpc, k5[9]);
			sha256_round((x[10] += sigma0(x[11]) + sigma1(x[8]) + x[3]), g, h, b, c, d, e, f, bpc, k5[10]);
			sha256_round((x[11] += sigma0(x[12]) + sigma1(x[9]) + x[4]), f, g, a, b, c, d, e, bpc, k5[11]);
			sha256_round((x[12] += sigma0(x[13]) + sigma1(x[10]) + x[5]), e, f, h, a, b, c, d, bpc, k5[12]);
			sha256_round((x[13] += sigma0(x[14]) + sigma1(x[11]) + x[6]), d, e, g, h, a, b, c, bpc, k5[13]);
			sha256_round((x[14] += sigma0(x[15]) + sigma1(x[12]) + x[7]), c, d, f, g, h, a, b, bpc, k5[14]);
			sha256_round((x[15] += sigma0(x[0]) + sigma1(x[13]) + x[8]), b, c, e, f, g, h, a, bpc, k5[15]);
		}
		a = (*state += a);
		b = (state[1] += b);
		c = (state[2] += c);
		d = (state[3] += d);
		e = (state[4] += e);
		f = (state[5] += f);
		g = (state[6] += g);
		h = (state[7] += h);
	}
}
