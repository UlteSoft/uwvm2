#pragma once

namespace fast_io
{

struct allocation_mmap_options
{
	::std::size_t extra_bytes{};
	bool write_back{};
	::fast_io::file_loader_padding_mode padding_mode{};

	inline explicit constexpr allocation_mmap_options() noexcept = default;
	inline constexpr allocation_mmap_options(::fast_io::file_loader_extra_bytes extra) noexcept
		: extra_bytes(extra.n), padding_mode(extra.mode)
	{}
	inline constexpr allocation_mmap_options(::fast_io::mmap_prot, ::fast_io::mmap_flags flagsv) noexcept
	{
		::std::uint_least32_t exclusiveflags{static_cast<::std::uint_least32_t>(flagsv & ::fast_io::mmap_flags::map_type)};
		if (exclusiveflags == 3u)
		{
			exclusiveflags = 1u;
		}
		if (exclusiveflags == 1u)
		{
			this->write_back = true;
		}
	}
	inline constexpr allocation_mmap_options(::fast_io::mmap_prot protv, ::fast_io::mmap_flags flagsv,
											 ::fast_io::file_loader_extra_bytes extra) noexcept
		: allocation_mmap_options(protv, flagsv)
	{
		this->extra_bytes = extra.n;
		this->padding_mode = extra.mode;
	}
};

} // namespace fast_io
