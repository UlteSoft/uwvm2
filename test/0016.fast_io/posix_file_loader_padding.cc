/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-28
 * @copyright   APL-2.0 License
 */

#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <fast_io.h>
#else
# error "Module testing is not currently supported"
#endif

#define UWVM_TEST_REQUIRE(expr) \
	do \
	{ \
		if(!(expr)) \
		{ \
			return __LINE__; \
		} \
	} while(false)

#if !defined(_WIN32) && !defined(__MSDOS__) && !defined(__wasi__) && !defined(__EMSCRIPTEN__)

namespace uwvm2_test_posix_file_loader_padding
{
	inline constexpr ::std::size_t extra_small{64uz};

	struct byte_buffer
	{
		::std::byte* data{};
		::std::size_t size{};

		inline explicit byte_buffer(::std::size_t n) : data{n == 0 ? nullptr : new ::std::byte[n]}, size{n} {}
		byte_buffer(byte_buffer const&) = delete;
		byte_buffer& operator=(byte_buffer const&) = delete;

		inline ~byte_buffer()
		{
			delete[] data;
		}
	};

	struct file_guard
	{
		char path[256]{};

		inline explicit file_guard()
		{
			char pattern[]{"/tmp/uwvm2_posix_file_loader_padding_XXXXXX"};
			int fd{mkstemp(pattern)};
			if(fd == -1)
			{
				::fast_io::throw_posix_error();
			}
			::fast_io::noexcept_call(::close, fd);
			char* out{path};
			char const* in{pattern};
			while((*out++ = *in++) != 0) {}
		}
		file_guard(file_guard const&) = delete;
		file_guard& operator=(file_guard const&) = delete;

		inline ~file_guard()
		{
			::fast_io::noexcept_call(::unlink, path);
		}
	};

	inline ::std::byte payload_byte(::std::size_t index) noexcept
	{
		return static_cast<::std::byte>((index * 149uz + 23uz) & 0xffuz);
	}

	inline ::std::byte expected_payload_byte(::std::size_t index, ::std::size_t payload_size,
											 bool shared_writeback) noexcept
	{
		if(shared_writeback && payload_size != 0)
		{
			if(index == 0)
			{
				return static_cast<::std::byte>(0x5a);
			}
			if(index + 1uz == payload_size)
			{
				return static_cast<::std::byte>(0x6b);
			}
		}
		return payload_byte(index);
	}

	inline ::std::size_t get_page_size() noexcept
	{
#if defined(_SC_PAGESIZE)
		long const ret{::fast_io::noexcept_call(::sysconf, _SC_PAGESIZE)};
		if(ret > 0)
		{
			return static_cast<::std::size_t>(ret);
		}
#endif
		return 4096uz;
	}

	inline ::std::size_t round_up_to_page(::std::size_t value, ::std::size_t page_size)
	{
		if(value == 0)
		{
			return 0;
		}
		auto const rem{value % page_size};
		return rem == 0 ? value : value + (page_size - rem);
	}

	inline int write_payload(char const* path, ::std::size_t payload_size)
	{
		::fast_io::posix_file file{::fast_io::mnp::os_c_str(path),
									::fast_io::open_mode::out | ::fast_io::open_mode::trunc};
		byte_buffer payload{payload_size};
		for(::std::size_t i{}; i != payload_size; ++i)
		{
			payload.data[i] = payload_byte(i);
		}
		if(payload_size != 0)
		{
			::fast_io::operations::decay::write_all_bytes_decay(
				::fast_io::posix_io_observer{file.fd}, payload.data, payload.data + payload_size);
		}
		return 0;
	}

	inline int file_size_is(char const* path, ::std::size_t expected_size)
	{
		::fast_io::posix_file file{::fast_io::mnp::os_c_str(path), ::fast_io::open_mode::in};
		UWVM_TEST_REQUIRE(::fast_io::file_size(file) == expected_size);
		return 0;
	}

	inline int file_content_is_payload(char const* path, ::std::size_t payload_size, bool shared_writeback)
	{
		::fast_io::posix_file file{::fast_io::mnp::os_c_str(path), ::fast_io::open_mode::in};
		byte_buffer bytes{payload_size};
		if(payload_size != 0)
		{
			::fast_io::operations::decay::read_all_bytes_decay(
				::fast_io::posix_io_observer{file.fd}, bytes.data, bytes.data + payload_size);
			for(::std::size_t i{}; i != payload_size; ++i)
			{
				UWVM_TEST_REQUIRE(bytes.data[i] == expected_payload_byte(i, payload_size, shared_writeback));
			}
		}
		return 0;
	}

	template <typename Loader, typename Options>
	inline int exercise_loader(char const* path, Options const& options, ::std::size_t payload_size,
							   ::std::size_t extra_size, bool expect_zero_padding)
	{
		Loader loader{options, ::fast_io::mnp::os_c_str(path)};
		UWVM_TEST_REQUIRE(loader.size() == payload_size);
		UWVM_TEST_REQUIRE(loader.capacity() == payload_size + extra_size);
		UWVM_TEST_REQUIRE(loader.padding_size() == extra_size);
		auto const* const data{loader.data()};
		if(payload_size + extra_size == 0)
		{
			return 0;
		}
		UWVM_TEST_REQUIRE(data != nullptr);
		for(::std::size_t i{}; i != payload_size; ++i)
		{
			UWVM_TEST_REQUIRE(static_cast<::std::byte>(data[i]) == payload_byte(i));
		}
		for(::std::size_t i{}; i != extra_size; ++i)
		{
			auto const value{data[payload_size + i]};
			if(expect_zero_padding)
			{
				UWVM_TEST_REQUIRE(value == 0);
			}
			static_cast<void>(value);
		}
		return 0;
	}

	template <typename Mapping>
	inline int verify_released_mapping(Mapping const& mapping, ::std::size_t payload_size,
									   ::std::size_t extra_size, bool expect_fd)
	{
		UWVM_TEST_REQUIRE(mapping.address_begin != nullptr);
		UWVM_TEST_REQUIRE(mapping.address_end == mapping.address_begin + payload_size);
		UWVM_TEST_REQUIRE(mapping.address_capacity == mapping.address_begin + payload_size + extra_size);
		if constexpr(requires { mapping.fd; })
		{
			UWVM_TEST_REQUIRE((mapping.fd != -1) == expect_fd);
		}
		else
		{
			static_cast<void>(expect_fd);
		}
		for(::std::size_t i{}; i != payload_size; ++i)
		{
			UWVM_TEST_REQUIRE(static_cast<::std::byte>(mapping.address_begin[i]) == payload_byte(i));
		}
		for(::std::size_t i{}; i != extra_size; ++i)
		{
			UWVM_TEST_REQUIRE(mapping.address_begin[payload_size + i] == 0);
		}
		return 0;
	}

	template <typename Loader, typename Options>
	inline int exercise_release_adopt(char const* path, Options const& options, ::fast_io::open_mode mode,
									  ::std::size_t payload_size, ::std::size_t extra_size,
									  bool expect_fd, bool shared_writeback)
	{
		{
			Loader loader{options, ::fast_io::mnp::os_c_str(path), mode};
			auto mapping{loader.release()};
			UWVM_TEST_REQUIRE(loader.size() == 0);
			UWVM_TEST_REQUIRE(loader.capacity() == 0);
			UWVM_TEST_REQUIRE(verify_released_mapping(mapping, payload_size, extra_size, expect_fd) == 0);

			{
				Loader adopted{mapping};
				UWVM_TEST_REQUIRE(adopted.data() == mapping.address_begin);
				UWVM_TEST_REQUIRE(adopted.size() == payload_size);
				UWVM_TEST_REQUIRE(adopted.capacity() == payload_size + extra_size);
				UWVM_TEST_REQUIRE(adopted.padding_size() == extra_size);
				if(payload_size != 0)
				{
					auto* const data{adopted.data()};
					data[0] = static_cast<char>(0x5a);
					data[payload_size - 1uz] = static_cast<char>(0x6b);
				}
				if(extra_size != 0)
				{
					auto* const data{adopted.data()};
					data[payload_size] = static_cast<char>(0x55);
					data[payload_size + extra_size - 1uz] = static_cast<char>(0x66);
				}
			}
		}
		UWVM_TEST_REQUIRE(file_size_is(path, payload_size) == 0);
		UWVM_TEST_REQUIRE(file_content_is_payload(path, payload_size, shared_writeback) == 0);
		return 0;
	}

	inline int exercise_release(::std::size_t page_size)
	{
		UWVM_TEST_REQUIRE(page_size > 16uz);
		auto const payload_size{page_size - 5uz};
		auto const extra_size{page_size + 19uz};
		auto const extra{::fast_io::file_loader_extra_bytes{extra_size}};

		file_guard posix_file;
		UWVM_TEST_REQUIRE(write_payload(posix_file.path, payload_size) == 0);
		auto const posix_options{::fast_io::posix_mmap_options{
			::fast_io::mmap_prot::prot_read | ::fast_io::mmap_prot::prot_write,
			::fast_io::mmap_flags::map_private,
			extra}};
		UWVM_TEST_REQUIRE(exercise_release_adopt<::fast_io::posix_file_loader>(
			posix_file.path, posix_options, ::fast_io::open_mode::in, payload_size, extra_size,
			false, false) == 0);

		file_guard allocation_private_file;
		UWVM_TEST_REQUIRE(write_payload(allocation_private_file.path, payload_size) == 0);
		auto const allocation_private_options{::fast_io::allocation_mmap_options{
			::fast_io::mmap_prot::prot_read | ::fast_io::mmap_prot::prot_write,
			::fast_io::mmap_flags::map_private,
			extra}};
		UWVM_TEST_REQUIRE(exercise_release_adopt<::fast_io::allocation_file_loader>(
			allocation_private_file.path, allocation_private_options, ::fast_io::open_mode::in,
			payload_size, extra_size, false, false) == 0);

		file_guard allocation_shared_file;
		UWVM_TEST_REQUIRE(write_payload(allocation_shared_file.path, payload_size) == 0);
		auto const allocation_shared_options{::fast_io::allocation_mmap_options{
			::fast_io::mmap_prot::prot_read | ::fast_io::mmap_prot::prot_write,
			::fast_io::mmap_flags::map_shared,
			extra}};
		UWVM_TEST_REQUIRE(exercise_release_adopt<::fast_io::allocation_file_loader>(
			allocation_shared_file.path, allocation_shared_options,
			::fast_io::open_mode::in | ::fast_io::open_mode::out,
			payload_size, extra_size, true, true) == 0);
		return 0;
	}

	inline int run()
	{
		auto const page_size{get_page_size()};
		::std::size_t const sizes[]{
			0uz,
			1uz,
			4uz,
			page_size - 1uz,
			page_size,
			page_size + 17uz,
			page_size * 2uz + 3uz};
		::std::size_t const extra_sizes[]{
			0uz,
			1uz,
			extra_small,
			page_size + 17uz};

		for(auto const payload_size : sizes)
		{
			file_guard file;
			UWVM_TEST_REQUIRE(write_payload(file.path, payload_size) == 0);
			for(auto const extra_size : extra_sizes)
			{
				auto const default_extra{::fast_io::file_loader_extra_bytes{extra_size}};
				auto const posix_options{::fast_io::posix_mmap_options{
					::fast_io::mmap_prot::prot_read | ::fast_io::mmap_prot::prot_write,
					::fast_io::mmap_flags::map_private,
					default_extra}};
				auto const allocation_options{::fast_io::allocation_mmap_options{default_extra}};
				UWVM_TEST_REQUIRE(exercise_loader<::fast_io::posix_file_loader>(
					file.path, posix_options, payload_size, extra_size, true) == 0);
				UWVM_TEST_REQUIRE(exercise_loader<::fast_io::allocation_file_loader>(
					file.path, allocation_options, payload_size, extra_size, true) == 0);

				auto const zero_extra{::fast_io::file_loader_extra_bytes{
					extra_size, ::fast_io::file_loader_padding_mode::zero}};
				auto const posix_zero_options{::fast_io::posix_mmap_options{
					::fast_io::mmap_prot::prot_read | ::fast_io::mmap_prot::prot_write,
					::fast_io::mmap_flags::map_private,
					zero_extra}};
				auto const allocation_zero_options{::fast_io::allocation_mmap_options{zero_extra}};
				UWVM_TEST_REQUIRE(exercise_loader<::fast_io::posix_file_loader>(
					file.path, posix_zero_options, payload_size, extra_size, true) == 0);
				UWVM_TEST_REQUIRE(exercise_loader<::fast_io::allocation_file_loader>(
					file.path, allocation_zero_options, payload_size, extra_size, true) == 0);

				auto const uninitialized_extra{::fast_io::file_loader_extra_bytes{
					extra_size, ::fast_io::file_loader_padding_mode::uninitialized}};
				auto const posix_uninitialized_options{::fast_io::posix_mmap_options{
					::fast_io::mmap_prot::prot_read | ::fast_io::mmap_prot::prot_write,
					::fast_io::mmap_flags::map_private,
					uninitialized_extra}};
				auto const allocation_uninitialized_options{
					::fast_io::allocation_mmap_options{uninitialized_extra}};
				UWVM_TEST_REQUIRE(exercise_loader<::fast_io::posix_file_loader>(
					file.path, posix_uninitialized_options, payload_size, extra_size, false) == 0);
				UWVM_TEST_REQUIRE(exercise_loader<::fast_io::allocation_file_loader>(
					file.path, allocation_uninitialized_options, payload_size, extra_size, false) == 0);

				static_cast<void>(round_up_to_page(payload_size, page_size));
			}
		}
		UWVM_TEST_REQUIRE(exercise_release(page_size) == 0);
		return 0;
	}
} // namespace uwvm2_test_posix_file_loader_padding

int main()
{
	try
	{
		return ::uwvm2_test_posix_file_loader_padding::run();
	}
	catch(...)
	{
		return 255;
	}
}

#else

int main()
{
	return 0;
}

#endif
