#pragma once

namespace fast_io
{
namespace containers
{
namespace details
{

struct
#if __has_cpp_attribute(__gnu__::__may_alias__)
	[[__gnu__::__may_alias__]]
#endif
	deque_control_block_common
{
	::std::byte *begin_ptr, *curr_ptr;
	::std::byte **controller_ptr;
};

template <typename T>
struct deque_control_block
{
	T *begin_ptr, *curr_ptr;
	T **controller_ptr;
};

inline constexpr ::std::size_t deque_block_size_shift{12};

inline constexpr ::std::size_t deque_block_size_common{static_cast<::std::size_t>(1) << deque_block_size_shift};

template <::std::size_t sz>
inline constexpr ::std::size_t deque_block_size{sz <= (deque_block_size_common / 16u) ? ::std::bit_ceil(static_cast<::std::size_t>(deque_block_size_common / sz)) : static_cast<::std::size_t>(16u)};

template <::std::size_t sz>
inline constexpr ::std::size_t deque_block_bytes{sz * ::fast_io::containers::details::deque_block_size<sz>};

struct
#if __has_cpp_attribute(__gnu__::__may_alias__)
	[[__gnu__::__may_alias__]]
#endif
	deque_controller_block_common
{
	using replacetype = ::std::byte;
	::std::byte **controller_start_ptr;
	::std::byte **controller_start_reserved_ptr;
	::std::byte **controller_after_reserved_ptr;
	::std::byte **controller_after_ptr;
};

template <typename T>
struct deque_controller_block
{
	using replacetype = T;
	T **controller_start_ptr;
	T **controller_start_reserved_ptr;
	T **controller_after_reserved_ptr;
	T **controller_after_ptr;
};

template <typename T>
struct deque_controller
{
	using replacetype = T;
	using controlreplacetype = T *;
	::fast_io::containers::details::deque_control_block<T> front_block;
	controlreplacetype front_end_ptr;
	::fast_io::containers::details::deque_control_block<T> back_block;
	controlreplacetype back_end_ptr;
	::fast_io::containers::details::deque_controller_block<T> controller_block;
};

struct
#if __has_cpp_attribute(__gnu__::__may_alias__)
	[[__gnu__::__may_alias__]]
#endif
	deque_controller_common
{
	using replacetype = ::std::byte;
	using controlreplacetype = ::std::byte *;
	::fast_io::containers::details::deque_control_block_common front_block;
	controlreplacetype front_end_ptr;
	::fast_io::containers::details::deque_control_block_common back_block;
	controlreplacetype back_end_ptr;
	::fast_io::containers::details::deque_controller_block_common controller_block;
};

template <typename T>
inline constexpr void deque_add_assign_signed_impl(
	::fast_io::containers::details::deque_control_block<T> &itercontent,
	::std::ptrdiff_t pos) noexcept
{
	constexpr ::std::size_t blocksize{::fast_io::containers::details::deque_block_size<sizeof(T)>};
	constexpr ::std::ptrdiff_t signedblocksize{static_cast<::std::ptrdiff_t>(blocksize)};

	auto curr_ptr{itercontent.curr_ptr};
	// Signed offset inside current block
	::std::ptrdiff_t offset{pos + (curr_ptr - itercontent.begin_ptr)};

	// Fast path: stays inside the same block
	if (static_cast<::std::size_t>(offset) < blocksize)
	{
		itercontent.curr_ptr = curr_ptr + pos;
	}
	else
	{
		// Compute block jump with correct signed semantics
		::std::ptrdiff_t node_offset;
		if (offset < 0)
		{
			node_offset = (offset + 1) / signedblocksize - 1;
		}
		else
		{
			node_offset = static_cast<::std::ptrdiff_t>(static_cast<::std::size_t>(offset) / blocksize);
		}

		// Move to the correct block
		auto begin_ptr{*(itercontent.controller_ptr += node_offset)};
		itercontent.begin_ptr = begin_ptr;

		// Normalize pointer inside the block
		itercontent.curr_ptr = begin_ptr + (offset - node_offset * signedblocksize);
	}
}

template <typename T>
inline constexpr void deque_sub_assign_signed_impl(::fast_io::containers::details::deque_control_block<T> &it, ::std::ptrdiff_t pos) noexcept
{
	::fast_io::containers::details::deque_add_assign_signed_impl(it, -pos);
}

template <typename T>
inline constexpr void deque_add_assign_unsigned_impl(::fast_io::containers::details::deque_control_block<T> &itercontent, ::std::size_t unsignedpos) noexcept
{
	using size_type = ::std::size_t;
	constexpr size_type blocksize{::fast_io::containers::details::deque_block_size<sizeof(T)>};

	auto begin_ptr{itercontent.begin_ptr};
	auto curr_ptr{itercontent.curr_ptr};
	size_type diff{static_cast<size_type>(curr_ptr - begin_ptr) + unsignedpos};
	if (diff < blocksize)
	{
		itercontent.curr_ptr = curr_ptr + unsignedpos;
	}
	else
	{
		begin_ptr = *(itercontent.controller_ptr += diff / blocksize);
		itercontent.begin_ptr = begin_ptr;
		itercontent.curr_ptr = begin_ptr + diff % blocksize;
	}
}

template <typename T>
inline constexpr void deque_sub_assign_unsigned_impl(::fast_io::containers::details::deque_control_block<T> &itercontent, ::std::size_t unsignedpos) noexcept
{
	using size_type = ::std::size_t;
	constexpr size_type blocksize{::fast_io::containers::details::deque_block_size<sizeof(T)>};
	constexpr size_type blocksizem1{blocksize - 1u};
	auto begin_ptr{itercontent.begin_ptr};
	auto curr_ptr{itercontent.curr_ptr};
	size_type offset{static_cast<size_type>(curr_ptr - begin_ptr)};
	if (unsignedpos <= offset)
	{
		itercontent.curr_ptr = curr_ptr - unsignedpos;
	}
	else
	{
		size_type diff{blocksizem1 + unsignedpos - offset};
		begin_ptr = (*(itercontent.controller_ptr -= diff / blocksize));
		itercontent.begin_ptr = begin_ptr;
		itercontent.curr_ptr = begin_ptr + (blocksizem1 - diff % blocksize);
	}
}

template <typename T>
inline constexpr T &deque_index_signed(::fast_io::containers::details::deque_control_block<T> const &itercontent, ::std::ptrdiff_t pos) noexcept
{
	using size_type = ::std::size_t;
	constexpr size_type blocksize{::fast_io::containers::details::deque_block_size<sizeof(T)>};
	constexpr ::std::ptrdiff_t signedblocksize{static_cast<::std::ptrdiff_t>(blocksize)};

	auto curr_ptr{itercontent.curr_ptr};
	// Signed offset inside current block
	::std::ptrdiff_t offset{pos + (curr_ptr - itercontent.begin_ptr)};

	// Fast path: stays inside the same block
	if (static_cast<::std::size_t>(offset) < blocksize)
	{
		return curr_ptr[pos];
	}
	else
	{
		// Compute block jump with correct signed semantics
		::std::ptrdiff_t node_offset;
		if (offset < 0)
		{
			node_offset = (offset + 1) / signedblocksize - 1;
		}
		else
		{
			node_offset = static_cast<::std::ptrdiff_t>(static_cast<::std::size_t>(offset) / blocksize);
		}
		return itercontent.controller_ptr[node_offset][offset - node_offset * signedblocksize];
	}
}

template <typename T>
inline constexpr T &deque_index_unsigned(::fast_io::containers::details::deque_control_block<T> const &itercontent, ::std::size_t unsignedpos) noexcept
{
	using size_type = ::std::size_t;
	constexpr size_type blocksize{::fast_io::containers::details::deque_block_size<sizeof(T)>};
	auto curr_ptr{itercontent.curr_ptr};
	size_type const diff{static_cast<size_type>(curr_ptr - itercontent.begin_ptr) + unsignedpos};
	if (diff < blocksize)
	{
		// Fast path: stays inside the same block
		return curr_ptr[unsignedpos];
	}
	else
	{
		return itercontent.controller_ptr[diff / blocksize][diff % blocksize];
	}
}

template <typename T>
inline constexpr T &deque_index_container_unsigned(::fast_io::containers::details::deque_control_block<T> const &itercontent, ::std::size_t unsignedpos) noexcept
{
	using size_type = ::std::size_t;
	constexpr size_type blocksize{::fast_io::containers::details::deque_block_size<sizeof(T)>};
	auto curr_ptr{itercontent.curr_ptr};
	size_type const diff{static_cast<size_type>(curr_ptr - itercontent.begin_ptr) + unsignedpos};
	// Container should not have fast path because container cannot have locality like iterator[] does
	return itercontent.controller_ptr[diff / blocksize][diff % blocksize];
}

template <typename T, bool isconst>
struct deque_iterator
{
	using value_type = T;
	using pointer = ::std::conditional_t<isconst, value_type const *, value_type *>;
	using const_pointer = value_type const *;

	using reference = ::std::conditional_t<isconst, value_type const &, value_type &>;
	using const_reference = value_type const &;

	using size_type = ::std::size_t;
	using difference_type = ::std::ptrdiff_t;

	deque_control_block<T> itercontent;

	inline constexpr deque_iterator &operator++() noexcept
	{
		if ((itercontent.begin_ptr + ::fast_io::containers::details::deque_block_size<sizeof(value_type)>) == ++itercontent.curr_ptr) [[unlikely]]
		{
			itercontent.curr_ptr = itercontent.begin_ptr = (*++itercontent.controller_ptr);
		}
		return *this;
	}

	inline constexpr deque_iterator &operator--() noexcept
	{
		if (itercontent.begin_ptr == itercontent.curr_ptr) [[unlikely]]
		{
			itercontent.curr_ptr = (itercontent.begin_ptr = (*--itercontent.controller_ptr)) + ::fast_io::containers::details::deque_block_size<sizeof(value_type)>;
		}
		--itercontent.curr_ptr;
		return *this;
	}

	inline constexpr deque_iterator operator++(int) noexcept
	{
		auto temp(*this);
		++*this;
		return temp;
	}

	inline constexpr deque_iterator operator--(int) noexcept
	{
		auto temp(*this);
		--*this;
		return temp;
	}

	inline constexpr reference operator*() const noexcept
	{
		return *this->itercontent.curr_ptr;
	}

	inline constexpr pointer operator->() const noexcept
	{
		return this->itercontent.curr_ptr;
	}
	template <::std::integral postype>
	inline constexpr deque_iterator &operator+=(postype pos) noexcept
	{
		if constexpr (::std::signed_integral<postype>)
		{
			::fast_io::containers::details::deque_add_assign_signed_impl(
				this->itercontent, static_cast<difference_type>(pos));
		}
		else
		{
			::fast_io::containers::details::deque_add_assign_unsigned_impl(
				this->itercontent, static_cast<size_type>(pos));
		}
		return *this;
	}
	template <::std::integral postype>
	inline constexpr deque_iterator &operator-=(postype pos) noexcept
	{
		if constexpr (::std::signed_integral<postype>)
		{
			::fast_io::containers::details::deque_sub_assign_signed_impl(
				this->itercontent, static_cast<difference_type>(pos));
		}
		else
		{
			::fast_io::containers::details::deque_sub_assign_unsigned_impl(
				this->itercontent, static_cast<size_type>(pos));
		}
		return *this;
	}
	template <::std::integral postype>
	inline constexpr reference operator[](postype pos) const noexcept
	{
		if constexpr (::std::signed_integral<postype>)
		{
			return ::fast_io::containers::details::deque_index_signed(this->itercontent,
																	  static_cast<difference_type>(pos));
		}
		else
		{
			return ::fast_io::containers::details::deque_index_unsigned(this->itercontent,
																		static_cast<size_type>(pos));
		}
	}

	inline constexpr operator deque_iterator<T, true>() const noexcept
		requires(!isconst)
	{
		return {this->itercontent};
	}
};

template <typename T, bool isconst, ::std::integral postype>
inline constexpr ::fast_io::containers::details::deque_iterator<T, isconst> operator+(::fast_io::containers::details::deque_iterator<T, isconst> a, postype pos) noexcept
{
	if constexpr (::std::signed_integral<postype>)
	{
		::fast_io::containers::details::deque_add_assign_signed_impl(
			a.itercontent, static_cast<::std::ptrdiff_t>(pos));
	}
	else
	{
		::fast_io::containers::details::deque_add_assign_unsigned_impl(
			a.itercontent, static_cast<::std::size_t>(pos));
	}
	return a;
}

template <typename T, bool isconst, ::std::integral postype>
inline constexpr ::fast_io::containers::details::deque_iterator<T, isconst> operator+(postype pos, ::fast_io::containers::details::deque_iterator<T, isconst> a) noexcept
{
	if constexpr (::std::signed_integral<postype>)
	{
		::fast_io::containers::details::deque_add_assign_signed_impl(
			a.itercontent, static_cast<::std::ptrdiff_t>(pos));
	}
	else
	{
		::fast_io::containers::details::deque_add_assign_unsigned_impl(
			a.itercontent, static_cast<::std::size_t>(pos));
	}
	return a;
}

template <typename T, bool isconst, ::std::integral postype>
inline constexpr ::fast_io::containers::details::deque_iterator<T, isconst> operator-(::fast_io::containers::details::deque_iterator<T, isconst> a, postype pos) noexcept
{
	if constexpr (::std::signed_integral<postype>)
	{
		::fast_io::containers::details::deque_sub_assign_signed_impl(
			a.itercontent, static_cast<::std::ptrdiff_t>(pos));
	}
	else
	{
		::fast_io::containers::details::deque_sub_assign_unsigned_impl(
			a.itercontent, static_cast<::std::size_t>(pos));
	}
	return a;
}

template <typename T>
inline constexpr ::std::ptrdiff_t deque_iter_difference_common(::fast_io::containers::details::deque_control_block<T> const &a, ::fast_io::containers::details::deque_control_block<T> const &b) noexcept
{
	::std::ptrdiff_t controllerdiff{a.controller_ptr - b.controller_ptr};
	constexpr ::std::ptrdiff_t blocksizedf{static_cast<::std::ptrdiff_t>(::fast_io::containers::details::deque_block_size<sizeof(T)>)};
	return controllerdiff * blocksizedf + (a.curr_ptr - a.begin_ptr) + (b.begin_ptr - b.curr_ptr);
}

template <typename T>
inline constexpr ::std::size_t deque_iter_difference_unsigned_common(::fast_io::containers::details::deque_control_block<T> const &a, ::fast_io::containers::details::deque_control_block<T> const &b) noexcept
{
	::std::size_t controllerdiff{static_cast<::std::size_t>(a.controller_ptr - b.controller_ptr)};
	constexpr ::std::size_t blocksizedf{::fast_io::containers::details::deque_block_size<sizeof(T)>};
	return controllerdiff * blocksizedf + static_cast<::std::size_t>((a.curr_ptr - a.begin_ptr) + (b.begin_ptr - b.curr_ptr));
}

template <typename T, bool isconst1, bool isconst2>
inline constexpr ::std::ptrdiff_t operator-(::fast_io::containers::details::deque_iterator<T, isconst1> const &a, ::fast_io::containers::details::deque_iterator<T, isconst2> const &b) noexcept
{
	return ::fast_io::containers::details::deque_iter_difference_common(a.itercontent, b.itercontent);
}

template <typename T, bool isconst1, bool isconst2>
inline constexpr bool operator==(::fast_io::containers::details::deque_iterator<T, isconst1> const &a, ::fast_io::containers::details::deque_iterator<T, isconst2> const &b) noexcept
{
	return a.itercontent.curr_ptr == b.itercontent.curr_ptr;
}

template <typename T, bool isconst1, bool isconst2>
inline constexpr auto operator<=>(::fast_io::containers::details::deque_iterator<T, isconst1> const &a, ::fast_io::containers::details::deque_iterator<T, isconst2> const &b) noexcept
{
	auto block3way{a.itercontent.controller_ptr <=> b.itercontent.controller_ptr};
	if (block3way == 0)
	{
		return a.itercontent.curr_ptr <=> b.itercontent.curr_ptr;
	}
	return block3way;
}

template <typename allocator, typename controllerblocktype>
inline constexpr void deque_destroy_trivial_common_align(controllerblocktype &controller, ::std::size_t aligns, ::std::size_t totalsz) noexcept
{
#if __cpp_if_consteval >= 202106L
	if consteval
	{
		if (controller.controller_start_ptr == nullptr)
		{
			return;
		}
		for (auto i{controller.controller_start_reserved_ptr}, e{controller.controller_after_reserved_ptr}; i != e; ++i)
		{
#if FAST_IO_HAS_BUILTIN(__builtin_operator_delete)
			__builtin_operator_delete(*i);
#else
			::operator delete(*i);
#endif
		}
		using controller_replacetype = typename controllerblocktype::replacetype;
		::std::size_t const element_count{static_cast<::std::size_t>(controller.controller_after_ptr - controller.controller_start_ptr + 1)};
		::fast_io::typed_generic_allocator_adapter<allocator, controller_replacetype *>::deallocate_n(controller.controller_start_ptr, element_count);
		return;
	}
	else
#endif
	{
		for (auto i{controller.controller_start_reserved_ptr}, e{controller.controller_after_reserved_ptr}; i != e; ++i)
		{
			allocator::deallocate_aligned_n(*i, aligns, totalsz);
		}
		::std::size_t const n{static_cast<::std::size_t>(controller.controller_after_ptr - controller.controller_start_ptr + 1) * sizeof(void *)};
		allocator::deallocate_n(controller.controller_start_ptr, n);
	}
}

template <typename allocator, ::std::size_t align, ::std::size_t sz, typename controllerblocktype>
inline constexpr void deque_destroy_trivial_common(controllerblocktype &controller) noexcept
{
	constexpr ::std::size_t totalsz{sz * ::fast_io::containers::details::deque_block_size<sz>};
	if consteval
	{
		::fast_io::containers::details::deque_destroy_trivial_common_align<allocator>(controller, align, totalsz);
	}
	else
	{
		::fast_io::containers::details::deque_destroy_trivial_common_align<allocator>(
			*reinterpret_cast<::fast_io::containers::details::deque_controller_block_common *>(__builtin_addressof(controller)), align, totalsz);
	}
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_grow_to_new_blocks_count_impl(dequecontroltype &controller, ::std::size_t new_blocks_count_least) noexcept
{
	auto old_start_ptr{controller.controller_block.controller_start_ptr};

	auto old_start_reserved_ptr{controller.controller_block.controller_start_reserved_ptr};
	auto old_after_reserved_ptr{controller.controller_block.controller_after_reserved_ptr};

	::std::size_t const old_start_reserved_ptr_pos{static_cast<::std::size_t>(old_start_reserved_ptr - old_start_ptr)};
	::std::size_t const old_after_ptr_pos{static_cast<::std::size_t>(controller.controller_block.controller_after_ptr - old_start_ptr)};
	::std::size_t const old_front_block_ptr_pos{static_cast<::std::size_t>(controller.front_block.controller_ptr - old_start_ptr)};
	::std::size_t const old_back_block_ptr_pos{static_cast<::std::size_t>(controller.back_block.controller_ptr - old_start_ptr)};

	using block_typed_allocator = ::fast_io::typed_generic_allocator_adapter<allocator, typename dequecontroltype::controlreplacetype>;

	::std::size_t new_blocks_count_least_p1;
#if (defined(__GNUC__) || defined(__clang__))
	if constexpr (true)
	{
		if (__builtin_add_overflow(new_blocks_count_least, 1zu, __builtin_addressof(new_blocks_count_least_p1))) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	}
	else
#endif
	{
		constexpr ::std::size_t mx{::std::numeric_limits<::std::size_t>::max()};
		new_blocks_count_least_p1 = new_blocks_count_least;
		if (mx == new_blocks_count_least)
		{
			::fast_io::fast_terminate();
		}
		++new_blocks_count_least_p1;
	}

	auto [new_start_ptr, new_blocks_count] = block_typed_allocator::allocate_at_least(new_blocks_count_least_p1);

	auto const old_reserved_blocks_count{
		static_cast<::std::size_t>(old_after_reserved_ptr - old_start_reserved_ptr)};
	auto const old_half_reserved_blocks_count{
		static_cast<::std::size_t>(old_reserved_blocks_count >> 1u)};
	auto old_reserved_pivot{old_start_reserved_ptr + old_half_reserved_blocks_count};
	auto const old_used_blocks_count{
		static_cast<::std::size_t>(controller.back_block.controller_ptr - controller.front_block.controller_ptr) + 1zu};
	auto const old_half_used_blocks_count{
		static_cast<::std::size_t>(old_used_blocks_count >> 1u)};
	auto old_used_blocks_pivot{controller.front_block.controller_ptr + old_half_used_blocks_count};

	::std::ptrdiff_t pivot_diff{old_reserved_pivot - old_used_blocks_pivot};

	::std::size_t const new_blocks_offset{static_cast<::std::size_t>(new_blocks_count - old_reserved_blocks_count) >> 1u};
	--new_blocks_count;

	auto new_start_reserved_ptr{new_start_ptr + new_blocks_offset};
	auto new_after_reserved_ptr{new_start_reserved_ptr + old_reserved_blocks_count};

	decltype(old_start_reserved_ptr) old_pivot, new_pivot;
	if (pivot_diff < 0)
	{
		old_pivot = old_start_reserved_ptr;
		new_pivot = new_after_reserved_ptr;
	}
	else
	{
		old_pivot = old_after_reserved_ptr;
		new_pivot = new_start_reserved_ptr;
	}
	old_pivot -= pivot_diff;
	new_pivot += pivot_diff;

	::fast_io::freestanding::non_overlapped_copy(old_pivot,
												 old_after_reserved_ptr, new_start_reserved_ptr);
	::fast_io::freestanding::non_overlapped_copy(old_start_reserved_ptr,
												 old_pivot, new_pivot);

	*new_after_reserved_ptr = 0;
	block_typed_allocator::deallocate_n(old_start_ptr, static_cast<::std::size_t>(old_after_ptr_pos + 1u));

	controller.controller_block.controller_start_ptr = new_start_ptr;
	controller.controller_block.controller_start_reserved_ptr = new_start_reserved_ptr;
	controller.controller_block.controller_after_reserved_ptr = new_after_reserved_ptr;
	controller.controller_block.controller_after_ptr = new_start_ptr + new_blocks_count;

	controller.front_block.controller_ptr = new_start_ptr + (new_blocks_offset + (old_front_block_ptr_pos - old_start_reserved_ptr_pos)) + pivot_diff;
	controller.back_block.controller_ptr = new_start_ptr + (new_blocks_offset + (old_back_block_ptr_pos - old_start_reserved_ptr_pos)) + pivot_diff;
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_allocate_on_empty_common_with_n_impl(dequecontroltype &controller, ::std::size_t initial_allocated_block_counts, ::std::size_t align, ::std::size_t bytes) noexcept
{
	::std::size_t initial_allocated_block_counts_with_sentinel;
#if (defined(__GNUC__) || defined(__clang__))
	if constexpr (true)
	{
		if (__builtin_add_overflow(initial_allocated_block_counts, 1u,
								   __builtin_addressof(initial_allocated_block_counts_with_sentinel)))
		{
			::fast_io::fast_terminate();
		}
	}
	else
#endif
	{
		constexpr ::std::size_t maxval{::std::numeric_limits<::std::size_t>::max()};
		if (initial_allocated_block_counts == maxval) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		initial_allocated_block_counts_with_sentinel = initial_allocated_block_counts + 1u;
	}
	using block_typed_allocator = ::fast_io::typed_generic_allocator_adapter<allocator, typename dequecontroltype::controlreplacetype>;
	auto [allocated_blocks_ptr, allocated_blocks_count] = block_typed_allocator::allocate_at_least(initial_allocated_block_counts_with_sentinel);

	// we need a null terminator as sentinel like c style string does
	--allocated_blocks_count;
	auto &controller_block{controller.controller_block};

	using replacetype = typename dequecontroltype::replacetype;
	using begin_ptrtype = replacetype *;

	controller_block.controller_after_ptr = (controller_block.controller_start_ptr = allocated_blocks_ptr) + allocated_blocks_count;

	::std::size_t const allocated_blocks_count_half{allocated_blocks_count >> 1u};
	::std::size_t const initial_allocated_block_counts_half{initial_allocated_block_counts >> 1u};

	auto allocated_mid_block{allocated_blocks_ptr + allocated_blocks_count_half};

	auto start_block_ptr{allocated_mid_block - initial_allocated_block_counts_half};

	auto end_block_ptr{start_block_ptr + initial_allocated_block_counts};
#if __cpp_if_consteval >= 202106L
	if consteval
	{
		for (auto i{start_block_ptr}; i != end_block_ptr; ++i)
		{
			::std::construct_at(i, static_cast<begin_ptrtype>(
#if FAST_IO_HAS_BUILTIN(__builtin_operator_new)
									   __builtin_operator_new(bytes * sizeof(replacetype))
#else
									   ::operator new(bytes * sizeof(replacetype))
#endif
										   ));
		}
	}
	else
#endif
	{
		for (auto i{start_block_ptr}; i != end_block_ptr; ++i)
		{
			::std::construct_at(i, static_cast<begin_ptrtype>(allocator::allocate_aligned(align, bytes * sizeof(replacetype))));
		}
	}
	::std::construct_at(end_block_ptr, static_cast<begin_ptrtype>(nullptr));
	controller_block.controller_start_reserved_ptr = start_block_ptr;
	controller_block.controller_after_reserved_ptr = end_block_ptr;
}


template <typename allocator, typename dequecontroltype>
inline constexpr void deque_allocate_empty_single_block_impl(dequecontroltype &controller, ::std::size_t align, ::std::size_t bytes) noexcept
{
	::fast_io::containers::details::deque_allocate_on_empty_common_with_n_impl<allocator>(controller, 1u, align, bytes);

	auto controllerptr{controller.controller_block.controller_start_reserved_ptr};
	auto begin_ptr{*controllerptr};
	auto mid_ptr{begin_ptr + (bytes >> 1u)};

	controller.front_block.begin_ptr = begin_ptr;
	controller.front_block.curr_ptr = mid_ptr;
	controller.front_block.controller_ptr = controllerptr;
	controller.back_block.begin_ptr = begin_ptr;
	controller.back_block.curr_ptr = mid_ptr;
	controller.back_block.controller_ptr = controllerptr;
	controller.front_end_ptr = controller.back_end_ptr = (begin_ptr + bytes);
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_clear_common_impl(dequecontroltype &controller, ::std::size_t blockbytes)
{
	auto start_reserved_ptr{controller.controller_block.controller_start_reserved_ptr};
	auto after_reserved_ptr{controller.controller_block.controller_after_reserved_ptr};
	if (start_reserved_ptr == after_reserved_ptr)
	{
		return;
	}
	auto const reserved_blocks_count{
		static_cast<::std::size_t>(after_reserved_ptr - start_reserved_ptr)};
	auto const half_reserved_blocks_count{
		static_cast<::std::size_t>(reserved_blocks_count >> 1u)};
	auto reserved_pivot{start_reserved_ptr + half_reserved_blocks_count};
	using replacetype = typename dequecontroltype::replacetype;
	using begin_ptrtype = replacetype *;
	auto begin_ptr{static_cast<begin_ptrtype>(*reserved_pivot)};
	auto end_ptr{begin_ptr + blockbytes};
	auto mid_ptr{begin_ptr + static_cast<::std::size_t>(blockbytes >> 1u)};
	controller.back_block.controller_ptr = controller.front_block.controller_ptr = reserved_pivot;
	controller.back_block.begin_ptr = controller.front_block.begin_ptr = begin_ptr;
	controller.back_block.curr_ptr = controller.front_block.curr_ptr = mid_ptr;
	controller.back_end_ptr = controller.front_end_ptr = end_ptr;
}

template <typename allocator, ::std::size_t sz, ::std::size_t block_size, typename dequecontroltype>
inline constexpr void deque_clear_common(dequecontroltype &controller) noexcept
{
	constexpr ::std::size_t blockbytes{sz * block_size};
	::fast_io::containers::details::deque_clear_common_impl<allocator>(controller, blockbytes);
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_allocate_init_blocks_dezeroing_impl(dequecontroltype &controller, ::std::size_t align, ::std::size_t blockbytes, ::std::size_t blocks_count_least, bool zeroing) noexcept
{
	if (!blocks_count_least)
	{
		controller = {};
		return;
	}
	constexpr ::std::size_t mx{::std::numeric_limits<::std::size_t>::max()};
	if (blocks_count_least == mx)
	{
		::fast_io::fast_terminate();
	}
	using block_typed_allocator = ::fast_io::typed_generic_allocator_adapter<allocator, typename dequecontroltype::controlreplacetype>;
	auto [start_ptr, blocks_count] = block_typed_allocator::allocate_at_least(blocks_count_least + 1u);
	--blocks_count;
	::std::size_t const half_blocks_count{blocks_count >> 1u};
	::std::size_t const half_blocks_count_least{blocks_count_least >> 1u};
	::std::size_t const offset{half_blocks_count - half_blocks_count_least};
	auto reserve_start{start_ptr + offset}, reserve_after{reserve_start + blocks_count_least};
	using replacetype = typename dequecontroltype::replacetype;
	using begin_ptrtype = replacetype *;
#if __cpp_if_consteval >= 202106L
	if consteval
	{
		for (auto it{reserve_start}, ed{reserve_after}; it != ed; ++it)
		{
			::std::construct_at(it, static_cast<begin_ptrtype>(
#if FAST_IO_HAS_BUILTIN(__builtin_operator_new)
										__builtin_operator_new(blockbytes * sizeof(replacetype))
#else
										::operator new(blockbytes * sizeof(replacetype))
#endif
											));
		}
	}
	else
#endif
	{
		for (auto it{reserve_start}, ed{reserve_after}; it != ed; ++it)
		{
			::std::construct_at(it, static_cast<begin_ptrtype>(allocator::allocate_aligned_conditional_zero(align, blockbytes * sizeof(replacetype), zeroing)));
		}
	}
	::std::construct_at(reserve_after, nullptr);
	begin_ptrtype reserve_start_block{static_cast<begin_ptrtype>(*reserve_start)};
	controller.front_block.begin_ptr = reserve_start_block;
	controller.front_block.curr_ptr = reserve_start_block;
	controller.front_block.controller_ptr = reserve_start;
	controller.front_end_ptr = reserve_start_block + blockbytes;
	begin_ptrtype reserve_back_block{static_cast<begin_ptrtype>(reserve_after[-1])};
	controller.back_block.begin_ptr = reserve_back_block;
	controller.back_block.curr_ptr = reserve_back_block;
	controller.back_block.controller_ptr = reserve_after - 1;
	controller.back_end_ptr = reserve_back_block + blockbytes;
	controller.controller_block.controller_start_ptr = start_ptr;
	controller.controller_block.controller_start_reserved_ptr = reserve_start;
	controller.controller_block.controller_after_reserved_ptr = reserve_after;
	controller.controller_block.controller_after_ptr = start_ptr + blocks_count;
}

template <typename allocator, ::std::size_t align, ::std::size_t sz, ::std::size_t block_size, typename dequecontroltype>
inline constexpr void deque_init_space_common(dequecontroltype &controller, ::std::size_t n, bool zeroing) noexcept
{
	constexpr ::std::size_t blockbytes{sz * block_size};
	::std::size_t const ndivsz{n / block_size};
	::std::size_t const nmodsz{n % block_size};
	::std::size_t const counts{ndivsz + static_cast<::std::size_t>(nmodsz != 0u)};

	::fast_io::containers::details::deque_allocate_init_blocks_dezeroing_impl<allocator>(controller, align, blockbytes, counts, zeroing);
	if (!n)
	{
		return;
	}
	auto &back_curr_ptr{controller.back_block.curr_ptr};
	::std::size_t offset_for_back{blockbytes};
	if (nmodsz)
	{
		offset_for_back = nmodsz * sz;
	}
	back_curr_ptr += offset_for_back;
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_shrink_to_fit_common(dequecontroltype &controller, ::std::size_t align, ::std::size_t block_bytes) noexcept
{
	if (controller.front_block.curr_ptr == controller.back_block.curr_ptr)
	{
		if (controller.front_block.curr_ptr == nullptr)
		{
			return;
		}
		::fast_io::containers::details::deque_destroy_trivial_common_align<allocator>(controller.controller_block, align, block_bytes);
		controller = dequecontroltype{};
		return;
	}

	auto &cb{controller.controller_block};
	auto start_reserved{cb.controller_start_reserved_ptr};
	auto const after_reserved{cb.controller_after_reserved_ptr};
	auto const front_ptr{controller.front_block.controller_ptr};
	auto const back_ptr{controller.back_block.controller_ptr};

	auto const start_ptr{cb.controller_start_ptr};
	auto const after_ptr{cb.controller_after_ptr + 1};

	// 1. Check if deallocation is needed for data blocks
	// We use after_reserved vs back_ptr + 1 safely by checking boundaries
	bool const needs_block_shrink = (start_reserved != front_ptr) || (after_reserved != (back_ptr + 1));

	if (needs_block_shrink)
	{
		for (; start_reserved != front_ptr; ++start_reserved)
		{
			allocator::deallocate_aligned_n(*start_reserved, align, block_bytes);
		}
		cb.controller_start_reserved_ptr = front_ptr;
		// it is safe to start from back_ptr + 1 because if we are here,
		// back_ptr must be within [start_ptr, after_ptr - 1]
		for (auto it{back_ptr + 1}; it != after_reserved; ++it)
		{
			allocator::deallocate_aligned_n(*it, align, block_bytes);
		}
		*(cb.controller_after_reserved_ptr = back_ptr + 1) = nullptr;
	}

	// 2. Controller Shrink Check
	// If blocks were already tight AND the controller array is already tight, return
	::std::size_t const used_blocks_count{static_cast<::std::size_t>(back_ptr + 1 - front_ptr)};
	::std::size_t const current_controller_size{static_cast<::std::size_t>(after_ptr - start_ptr)};

	if (!needs_block_shrink && current_controller_size <= (used_blocks_count + 1u))
	{
		return;
	}

	if constexpr (true)
	{
		// 3. Reallocate Controller
		::std::size_t const new_controller_size_least{used_blocks_count + 1u};
		// 3. Manually Reallocate and Move Controller Array
		using block_ptr_allocator = ::fast_io::typed_generic_allocator_adapter<allocator, typename dequecontroltype::controlreplacetype>;

		// Allocate new, smaller controller array
		auto [new_start_ptr, new_actual_count] = block_ptr_allocator::allocate_at_least(new_controller_size_least);

		// Move the pointers (non-overlapped because it's a new allocation)
		::fast_io::freestanding::non_overlapped_copy_n(front_ptr, used_blocks_count, new_start_ptr);

		// 4. Update controller metadata and pointers
		// We align the new reserved range to the start of the new allocation
		cb.controller_start_ptr = new_start_ptr;
		cb.controller_start_reserved_ptr = new_start_ptr;
		cb.controller_after_reserved_ptr = new_start_ptr + used_blocks_count;
		cb.controller_after_ptr = new_start_ptr + new_actual_count - 1u;

		// Patch the front and back block references
		controller.front_block.controller_ptr = new_start_ptr;
		controller.back_block.controller_ptr = new_start_ptr + (used_blocks_count - 1u);

		// 5. Set sentinel and clean up old controller
		*(cb.controller_after_reserved_ptr) = nullptr;
		block_ptr_allocator::deallocate_n(start_ptr, current_controller_size);
	}
}

template <typename allocator, ::std::size_t align, ::std::size_t blockbytes, typename dequecontroltype>
inline constexpr void deque_shrink_to_fit_impl(dequecontroltype &controller) noexcept
{
	::fast_io::containers::details::deque_shrink_to_fit_common<allocator>(controller, align, blockbytes);
}

template <typename ToIter>
struct uninitialized_copy_n_for_deque_guard
{
	bool torecover{true};
	ToIter d_first, &current;
	constexpr explicit uninitialized_copy_n_for_deque_guard(ToIter &toiter) noexcept
		: d_first(toiter), current(toiter)
	{}
	uninitialized_copy_n_for_deque_guard(uninitialized_copy_n_for_deque_guard const &) = delete;
	uninitialized_copy_n_for_deque_guard &operator=(uninitialized_copy_n_for_deque_guard const &) = delete;
	constexpr ~uninitialized_copy_n_for_deque_guard()
	{
		if (torecover)
		{
			::std::destroy(d_first, current);
		}
	}
};

template <typename FromIter, typename ToIter>
struct uninitialized_copy_n_for_deque_in_out_result
{
	FromIter from;
	ToIter to;
};
template <typename FromIter, typename ToIter>
inline constexpr ::fast_io::containers::details::uninitialized_copy_n_for_deque_in_out_result<FromIter, ToIter> uninitialized_copy_n_for_deque(FromIter fromiter, ::std::size_t count, ToIter toiter)
{
#if 0
	using fromvaluet = ::std::iter_value_t<FromIter>;
	using tovaluet = ::std::iter_value_t<ToIter>;
	if constexpr(::std::is_trivially_copyable_v<FromIter>&&
		::std::is_trivially_copyable_v<ToIter>)
	{
		
	}
#endif
	{
		uninitialized_copy_n_for_deque_guard g(toiter);
		for (; count; --count)
		{
			::std::construct_at(::std::addressof(*toiter), *fromiter);
			++fromiter;
			++toiter;
		}
		g.torecover = false;
	}
	return {fromiter, toiter};
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_clone_trivial_impl(dequecontroltype &controller, dequecontroltype const &fromcontroller,
											   ::std::size_t align, ::std::size_t blockbytes) noexcept
{
	if (fromcontroller.front_block.curr_ptr == fromcontroller.back_block.curr_ptr)
	{
		controller = {};
		return;
	}
	auto front_controller_ptr{fromcontroller.front_block.controller_ptr};
	auto back_controller_ptr{fromcontroller.back_block.controller_ptr};
	::std::size_t blocks_required{static_cast<::std::size_t>(back_controller_ptr -
															 front_controller_ptr + 1)};

	::fast_io::containers::details::deque_allocate_init_blocks_dezeroing_impl<allocator>(controller, align, blockbytes, blocks_required, false);
	using replacetype = typename dequecontroltype::replacetype;
	using begin_ptrtype = replacetype *;

	begin_ptrtype lastblockbegin;
	if (front_controller_ptr == back_controller_ptr)
	{
		lastblockbegin = fromcontroller.front_block.curr_ptr;
	}
	else
	{
		auto destit{controller.front_block.controller_ptr};
		auto pos{fromcontroller.front_block.curr_ptr - fromcontroller.front_block.begin_ptr};
		controller.front_end_ptr =
			::fast_io::freestanding::non_overlapped_copy(fromcontroller.front_block.curr_ptr,
														 fromcontroller.front_end_ptr,
														 (controller.front_block.curr_ptr =
															  pos + controller.front_block.begin_ptr));
		++destit;
		for (begin_ptrtype *it{front_controller_ptr + 1}, *ed{back_controller_ptr}; it != ed; ++it)
		{
			begin_ptrtype blockptr{*it};
			::fast_io::freestanding::non_overlapped_copy_n(blockptr, blockbytes, *destit);
			++destit;
		}
		lastblockbegin = fromcontroller.back_block.begin_ptr;
	}
	controller.back_block.curr_ptr =
		::fast_io::freestanding::non_overlapped_copy(lastblockbegin,
													 fromcontroller.back_block.curr_ptr, controller.back_block.begin_ptr);
}

template <typename allocator, ::std::size_t align, ::std::size_t sz, ::std::size_t block_size, typename dequecontroltype>
inline constexpr void deque_clone_trivial_common(dequecontroltype &controller, dequecontroltype const &fromcontroller) noexcept
{
	constexpr ::std::size_t blockbytes{sz * block_size};
	::fast_io::containers::details::deque_clone_trivial_impl<allocator>(controller, fromcontroller, align, blockbytes);
}

template <typename Itercontent>
inline constexpr Itercontent
deque_copy_impl(Itercontent first, Itercontent last,
				Itercontent dest, ::std::size_t blocksize)
{
	if (first.curr_ptr == last.curr_ptr)
	{
		return dest;
	}
	for (;;)
	{
		decltype(first.begin_ptr) firstend;
		if (first.controller_ptr == last.controller_ptr)
		{
			firstend = last.curr_ptr;
		}
		else
		{
			firstend = first.begin_ptr + blocksize;
		}
		auto firstcurrptr{first.curr_ptr};
		::std::size_t const curr_block_to_end{
			static_cast<::std::size_t>(firstend - firstcurrptr)};
		auto destcurrptr{dest.curr_ptr};
		::std::size_t const dest_block_to_end{
			static_cast<::std::size_t>(dest.begin_ptr + blocksize - destcurrptr)};
		auto cmp{curr_block_to_end <=> dest_block_to_end};
		::std::size_t tocopy;
		if (cmp < 0)
		{
			tocopy = curr_block_to_end;
		}
		else
		{
			tocopy = dest_block_to_end;
		}
		::fast_io::freestanding::copy_n(firstcurrptr, tocopy, destcurrptr);
		if (0 < cmp)
		{
			dest.curr_ptr = (dest.begin_ptr = (*++dest.controller_ptr));
			first.curr_ptr += tocopy;
		}
		else
		{
			if (cmp < 0)
			{
				dest.curr_ptr += tocopy;
			}
			else if (cmp == 0)
			{
				dest.curr_ptr = (dest.begin_ptr = (*++dest.controller_ptr));
			}
			if (first.controller_ptr == last.controller_ptr)
			{
				break;
			}
			first.curr_ptr = (first.begin_ptr = (*++first.controller_ptr));
		}
	}
	return dest;
}

template <typename Itercontent>
inline constexpr Itercontent
deque_copy_backward_impl(Itercontent first, Itercontent last,
						 Itercontent dest, ::std::size_t blocksize)
{
	if (first.curr_ptr == last.curr_ptr)
	{
		return dest;
	}
	for (;;)
	{
		decltype(first.begin_ptr) lastbegin;
		if (first.controller_ptr == last.controller_ptr)
		{
			lastbegin = first.curr_ptr;
		}
		else
		{
			lastbegin = last.begin_ptr;
		}
		auto lastcurrptr{last.curr_ptr};
		::std::size_t const curr_block_to_begin{
			static_cast<::std::size_t>(lastcurrptr - lastbegin)};
		auto destcurrptr{dest.curr_ptr};
		::std::size_t const dest_block_to_begin{
			static_cast<::std::size_t>(destcurrptr - dest.begin_ptr)};
		auto cmp{curr_block_to_begin <=> dest_block_to_begin};
		::std::size_t tocopy;
		if (cmp < 0)
		{
			tocopy = curr_block_to_begin;
		}
		else
		{
			tocopy = dest_block_to_begin;
		}
		::fast_io::freestanding::copy_backward(lastcurrptr - tocopy, lastcurrptr,
											   destcurrptr);
		if (0 < cmp)
		{
			dest.curr_ptr = (dest.begin_ptr = (*--dest.controller_ptr)) + blocksize;
			last.curr_ptr -= tocopy;
		}
		else
		{
			if (cmp == 0)
			{
				if (last.controller_ptr == first.controller_ptr)
				{
					dest.curr_ptr = dest.begin_ptr;
					break;
				}
				dest.curr_ptr = (dest.begin_ptr = (*--dest.controller_ptr)) + blocksize;
			}
			else
			{
				dest.curr_ptr -= tocopy;
				if (last.controller_ptr == first.controller_ptr)
				{
					break;
				}
			}
			last.curr_ptr = (last.begin_ptr = (*--last.controller_ptr)) + blocksize;
		}
	}
	return dest;
}


template <typename Itercontent>
inline constexpr Itercontent
deque_relocate_impl(Itercontent first, Itercontent last,
					Itercontent dest, ::std::size_t blocksize)
{
	if (first.curr_ptr == last.curr_ptr)
	{
		return dest;
	}
	for (;;)
	{
		decltype(first.begin_ptr) firstend;
		if (first.controller_ptr == last.controller_ptr)
		{
			firstend = last.curr_ptr;
		}
		else
		{
			firstend = first.begin_ptr + blocksize;
		}
		auto firstcurrptr{first.curr_ptr};
		::std::size_t const curr_block_to_end{
			static_cast<::std::size_t>(firstend - firstcurrptr)};
		auto destcurrptr{dest.curr_ptr};
		::std::size_t const dest_block_to_end{
			static_cast<::std::size_t>(dest.begin_ptr + blocksize - destcurrptr)};
		auto cmp{curr_block_to_end <=> dest_block_to_end};
		::std::size_t tocopy;
		if (cmp < 0)
		{
			tocopy = curr_block_to_end;
		}
		else
		{
			tocopy = dest_block_to_end;
		}
		::fast_io::freestanding::uninitialized_relocate_ignore_define(firstcurrptr, firstcurrptr + tocopy, destcurrptr);
		if (0 < cmp)
		{
			dest.curr_ptr = (dest.begin_ptr = (*++dest.controller_ptr));
			first.curr_ptr += tocopy;
		}
		else
		{
			if (cmp < 0)
			{
				dest.curr_ptr += tocopy;
			}
			else if (cmp == 0)
			{
				dest.curr_ptr = (dest.begin_ptr = (*++dest.controller_ptr));
			}
			if (first.controller_ptr == last.controller_ptr)
			{
				break;
			}
			first.curr_ptr = (first.begin_ptr = (*++first.controller_ptr));
		}
	}
	return dest;
}

template <typename Itercontent>
inline constexpr Itercontent
deque_relocate_backward_impl(Itercontent first, Itercontent last,
							 Itercontent dest, ::std::size_t blocksize)
{
	if (first.curr_ptr == last.curr_ptr)
	{
		return dest;
	}
	for (;;)
	{
		decltype(first.begin_ptr) lastbegin;
		if (first.controller_ptr == last.controller_ptr)
		{
			lastbegin = first.curr_ptr;
		}
		else
		{
			lastbegin = last.begin_ptr;
		}
		auto lastcurrptr{last.curr_ptr};
		::std::size_t const curr_block_to_begin{
			static_cast<::std::size_t>(lastcurrptr - lastbegin)};
		auto destcurrptr{dest.curr_ptr};
		::std::size_t const dest_block_to_begin{
			static_cast<::std::size_t>(destcurrptr - dest.begin_ptr)};
		auto cmp{curr_block_to_begin <=> dest_block_to_begin};
		::std::size_t tocopy;
		if (cmp < 0)
		{
			tocopy = curr_block_to_begin;
		}
		else
		{
			tocopy = dest_block_to_begin;
		}
		::fast_io::freestanding::uninitialized_relocate_backward_ignore_define(lastcurrptr - tocopy, lastcurrptr,
																			   destcurrptr);
		if (0 < cmp)
		{
			dest.curr_ptr = (dest.begin_ptr = (*--dest.controller_ptr)) + blocksize;
			last.curr_ptr -= tocopy;
		}
		else
		{
			if (cmp == 0)
			{
				if (last.controller_ptr == first.controller_ptr)
				{
					dest.curr_ptr = dest.begin_ptr;
					break;
				}
				dest.curr_ptr = (dest.begin_ptr = (*--dest.controller_ptr)) + blocksize;
			}
			else
			{
				dest.curr_ptr -= tocopy;
				if (last.controller_ptr == first.controller_ptr)
				{
					break;
				}
			}
			last.curr_ptr = (last.begin_ptr = (*--last.controller_ptr)) + blocksize;
		}
	}
	return dest;
}

inline ::fast_io::containers::details::deque_control_block_common
deque_erase_common_trivial_impl(::fast_io::containers::details::deque_controller_common &controller,
								::fast_io::containers::details::deque_control_block_common first,
								::fast_io::containers::details::deque_control_block_common last,
								bool moveleft,
								::std::size_t blockbytes) noexcept
{
	::fast_io::containers::details::deque_control_block_common back_block{controller.back_block};
	if (moveleft)
	{
		controller.front_block = ::fast_io::containers::details::deque_copy_backward_impl(controller.front_block, first, last, blockbytes);
		controller.front_end_ptr = controller.front_block.begin_ptr + blockbytes;
		first = last;
		if (controller.front_block.curr_ptr == back_block.curr_ptr && back_block.curr_ptr == controller.back_end_ptr) [[unlikely]]
		{
			auto back_begin_ptr{back_block.begin_ptr};
			if (back_begin_ptr) [[likely]]
			{
				// erase after empty we should reset to middle
				controller.front_block.curr_ptr = back_block.curr_ptr = back_begin_ptr + (blockbytes >> 1u);
			}
		}
	}
	else
	{
		if (back_block.curr_ptr == controller.back_end_ptr) [[unlikely]]
		{
			if (back_block.controller_ptr) [[likely]]
			{
				back_block.curr_ptr = back_block.begin_ptr = (*++back_block.controller_ptr);
			}
		}
		back_block = ::fast_io::containers::details::deque_copy_impl(last, back_block, first, blockbytes);
	}
	if (back_block.begin_ptr == back_block.curr_ptr)
	{
		if (controller.front_block.controller_ptr == back_block.controller_ptr)
		{
			controller.front_block.curr_ptr = back_block.curr_ptr = (back_block.begin_ptr) + (blockbytes >> 1u);
		}
		else
		{
			back_block.curr_ptr = ((back_block.begin_ptr = (*--back_block.controller_ptr)) + blockbytes);
		}
	}
	controller.back_block = back_block;
	controller.back_end_ptr = back_block.begin_ptr + blockbytes;
	return first;
}

inline constexpr ::std::size_t deque_new_blocks_count_compute_impl(::std::size_t new_blocks_count_least, ::std::size_t blocks, ::std::size_t reserved_space_at_opposite_direction) noexcept
{
#if (defined(__GNUC__) || defined(__clang__))
	if constexpr (true)
	{
		::std::size_t blocksx2;
		if (__builtin_add_overflow(blocks, blocks, __builtin_addressof(blocksx2)))
		{
			::fast_io::fast_terminate();
		}
		if (reserved_space_at_opposite_direction < blocksx2)
		{
			reserved_space_at_opposite_direction = blocksx2;
		}
		if (__builtin_add_overflow(new_blocks_count_least, reserved_space_at_opposite_direction, __builtin_addressof(reserved_space_at_opposite_direction)))
		{
			::fast_io::fast_terminate();
		}
		if (__builtin_add_overflow(reserved_space_at_opposite_direction, 1u, __builtin_addressof(reserved_space_at_opposite_direction)))
		{
			::fast_io::fast_terminate();
		}
		return reserved_space_at_opposite_direction;
	}
	else
#endif
	{
		::std::size_t blocksx2{blocks + blocks};
		if (blocksx2 < blocks)
		{
			::fast_io::fast_terminate();
		}
		if (reserved_space_at_opposite_direction < blocksx2)
		{
			reserved_space_at_opposite_direction = blocksx2;
		}
		constexpr ::std::size_t mx{::std::numeric_limits<::std::size_t>::max()};
		::std::size_t const mxm{mx - new_blocks_count_least};
		if (mxm < reserved_space_at_opposite_direction)
		{
			::fast_io::fast_terminate();
		}
		reserved_space_at_opposite_direction += new_blocks_count_least;
		if (mx == reserved_space_at_opposite_direction)
		{
			::fast_io::fast_terminate();
		}
		++reserved_space_at_opposite_direction;
		return reserved_space_at_opposite_direction;
	}
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_grow_to_new_blocks_count_direction_impl(dequecontroltype &controller, ::std::size_t new_blocks_count_least, bool no_space_at_back) noexcept
{
	auto old_start_ptr{controller.controller_block.controller_start_ptr};
#if __has_cpp_attribute(assume)
	[[assume(old_start_ptr != nullptr)]];
#endif
	auto old_after_ptr{controller.controller_block.controller_after_ptr};
	auto old_start_reserved_ptr{controller.controller_block.controller_start_reserved_ptr};
	auto old_after_reserved_ptr{controller.controller_block.controller_after_reserved_ptr};
	auto old_front_block_controller_ptr{controller.front_block.controller_ptr};
	auto old_back_block_controller_ptr{controller.back_block.controller_ptr};
	auto old_back_block_controller_ptr_p1{old_back_block_controller_ptr + 1};

	::std::size_t old_start_reserved_pos{
		static_cast<::std::size_t>(old_start_reserved_ptr - old_start_ptr)};
	::std::size_t old_after_reserved_pos{
		static_cast<::std::size_t>(old_after_reserved_ptr - old_start_ptr)};

	::std::size_t old_front_block_controller_pos{
		static_cast<::std::size_t>(old_front_block_controller_ptr - old_start_ptr)};
	::std::size_t old_back_block_controller_pos{
		static_cast<::std::size_t>(old_back_block_controller_ptr - old_start_ptr)};

	::std::size_t old_blocks_count{static_cast<::std::size_t>(old_back_block_controller_ptr_p1 - old_front_block_controller_ptr)};
#if 0
	::fast_io::io::debug_perr(::std::source_location::current(), "\n"
	"old_start_ptr=",::fast_io::mnp::pointervw(old_start_ptr),"\n"
	"old_after_pos=",old_after_ptr-old_start_ptr,"\n"
	"old_start_reserved_pos=",old_start_reserved_pos,"\n"
	"old_after_reserved_pos=",old_after_reserved_pos,"\n"
	"old_front_block_controller_pos=",old_front_block_controller_pos,"\n"
	"old_back_block_controller_pos=",old_back_block_controller_pos,"\n"
	"old_blocks_count=",old_blocks_count,"\n"
	"new_blocks_count_least=",new_blocks_count_least,"\n\n"
	);
#endif
	::std::size_t to_allocated_blocks_least_p1{::fast_io::containers::details::deque_new_blocks_count_compute_impl(new_blocks_count_least,
																												   old_blocks_count, (no_space_at_back ? static_cast<::std::size_t>(old_back_block_controller_ptr_p1 - old_start_reserved_ptr) : static_cast<::std::size_t>(old_after_reserved_ptr - old_front_block_controller_ptr)))};
	using block_typed_allocator = ::fast_io::typed_generic_allocator_adapter<allocator, typename dequecontroltype::controlreplacetype>;
	decltype(old_start_ptr) new_start_ptr;
	::std::size_t allocated_blocks;

	::std::size_t old_allocated_sz{static_cast<::std::size_t>(old_after_ptr - old_start_ptr)};
	::std::size_t old_allocated_sz_p1{old_allocated_sz + 1zu};
	bool is_allocating_new_block{old_allocated_sz_p1 < to_allocated_blocks_least_p1};
#if 0
	::fast_io::io::debug_perrln(::std::source_location::current(), "\tto_allocated_blocks_least_p1=", to_allocated_blocks_least_p1,
	"\tnew_blocks_count_least=",new_blocks_count_least);
#endif
	if (is_allocating_new_block)
	{
		auto allocate_result = block_typed_allocator::allocate_at_least(to_allocated_blocks_least_p1);
		new_start_ptr = allocate_result.ptr;
		allocated_blocks = allocate_result.count;
		--allocated_blocks;
	}
	else
	{
		new_start_ptr = old_start_ptr;
		allocated_blocks = old_allocated_sz;
	}
	::std::size_t new_start_reserved_ptr_pos;
	if (no_space_at_back)
	{
		new_start_reserved_ptr_pos = allocated_blocks - new_blocks_count_least - static_cast<::std::size_t>(old_front_block_controller_pos - old_start_reserved_pos);
	}
	else
	{
		new_start_reserved_ptr_pos = new_blocks_count_least - static_cast<::std::size_t>(old_back_block_controller_pos - old_start_reserved_pos);
	}
	auto new_start_reserved_ptr{new_start_ptr + new_start_reserved_ptr_pos};
	auto ed{::fast_io::freestanding::overlapped_copy(old_start_ptr + old_start_reserved_pos, old_start_ptr + old_after_reserved_pos, new_start_reserved_ptr)};
	::std::construct_at(ed, nullptr);

	controller.controller_block.controller_start_ptr = new_start_ptr;
	controller.controller_block.controller_after_ptr = new_start_ptr + allocated_blocks;
	controller.controller_block.controller_start_reserved_ptr = new_start_reserved_ptr;
	controller.controller_block.controller_after_reserved_ptr = ed;
	controller.front_block.controller_ptr = new_start_reserved_ptr + static_cast<::std::size_t>(old_front_block_controller_pos - old_start_reserved_pos);
	controller.back_block.controller_ptr = new_start_reserved_ptr + static_cast<::std::size_t>(old_back_block_controller_pos - old_start_reserved_pos);
	if (is_allocating_new_block)
	{
		block_typed_allocator::deallocate_n(old_start_ptr, old_allocated_sz_p1);
	}
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_rebalance_or_grow_insertation_direction_impl(dequecontroltype &controller, ::std::size_t extrablocks, bool no_space_at_back) noexcept
{
	auto old_front_controller_ptr{controller.front_block.controller_ptr};
	auto old_back_controller_ptr{controller.back_block.controller_ptr};
	auto old_back_after_controller_ptr{old_back_controller_ptr + 1zu};
	::std::size_t const old_elements_blocks_count{static_cast<::std::size_t>(old_back_after_controller_ptr - old_front_controller_ptr)};
	::std::size_t new_elements_blocks_count;

#if (defined(__GNUC__) || defined(__clang__))
	if constexpr (true)
	{
		if (__builtin_add_overflow(old_elements_blocks_count, extrablocks, __builtin_addressof(new_elements_blocks_count))) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	}
	else
#endif
	{
		constexpr ::std::size_t mx{::std::numeric_limits<::std::size_t>::max()};
		::std::size_t const mx_sub_extrablocks{mx - extrablocks};
		if (mx_sub_extrablocks < old_elements_blocks_count)
		{
			::fast_io::fast_terminate();
		}
		new_elements_blocks_count = old_elements_blocks_count + extrablocks;
	}
	auto start_ptr{controller.controller_block.controller_start_ptr};
	auto after_ptr{controller.controller_block.controller_after_ptr};
	::std::size_t capacity_blocks_count_direction{
		no_space_at_back ? static_cast<::std::size_t>(after_ptr - old_front_controller_ptr) : static_cast<::std::size_t>(old_back_after_controller_ptr - start_ptr)};

#if (defined(__GNUC__) || defined(__clang__))
	if constexpr (true)
	{
		if (__builtin_add_overflow(capacity_blocks_count_direction, capacity_blocks_count_direction, __builtin_addressof(capacity_blocks_count_direction)))
		{
			::fast_io::fast_terminate();
		}
	}
	else
#endif
	{
		constexpr ::std::size_t mx{::std::numeric_limits<::std::size_t>::max()};
		constexpr ::std::size_t mxdv2m1{(mx >> 1u)};
		if (mxdv2m1 < capacity_blocks_count_direction)
		{
			::fast_io::fast_terminate();
		}
		capacity_blocks_count_direction <<= 1u;
	}
	if (capacity_blocks_count_direction < new_elements_blocks_count)
	{
		capacity_blocks_count_direction = new_elements_blocks_count;
	}
	return ::fast_io::containers::details::deque_grow_to_new_blocks_count_direction_impl<allocator, dequecontroltype>(controller, capacity_blocks_count_direction, no_space_at_back);
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_reserve_back_blocks_impl_none_empty(dequecontroltype &controller, ::std::size_t nb, ::std::size_t align, ::std::size_t blockbytes) noexcept
{
	using replacetype = typename dequecontroltype::replacetype;
	using begin_ptrtype = replacetype *;

	::std::size_t diff_to_after_ptr =
		static_cast<::std::size_t>(
			controller.controller_block.controller_after_reserved_ptr -
			controller.back_block.controller_ptr);
#if 0
	::fast_io::io::debug_perr(::std::source_location::current(),
		"\ndiff_to_after_ptr=",diff_to_after_ptr,
		"\nnb=",nb,"\n\n");
#endif
	if (diff_to_after_ptr <= nb)
	{
		::std::size_t distance_back_to_after{
			static_cast<::std::size_t>(controller.controller_block.controller_after_ptr -
									   controller.back_block.controller_ptr)};
		if (distance_back_to_after <= nb)
		{
			::fast_io::containers::details::deque_rebalance_or_grow_insertation_direction_impl<allocator>(controller, nb, true);
		}
		::std::size_t diff_to_after_ptr2 =
			static_cast<::std::size_t>(
				controller.controller_block.controller_after_reserved_ptr -
				controller.back_block.controller_ptr);
		if (diff_to_after_ptr2 <= nb)
		{
			::std::size_t front_reserved_blocks{
				static_cast<::std::size_t>(controller.front_block.controller_ptr - controller.controller_block.controller_start_reserved_ptr)};

			::std::size_t front_borrowed_blocks_count{front_reserved_blocks};
			::std::size_t to_allocate_blocks{static_cast<::std::size_t>(nb - (diff_to_after_ptr2 ? diff_to_after_ptr2 - 1zu : 0zu))};
#if 0
			::fast_io::io::debug_perrln(::std::source_location::current(),
				"\nto_allocate_blocks=",to_allocate_blocks,
				"\tnb=",nb,
				"\tcontroller.front_block.controller_ptr-controller.controller_block.controller_start_ptr=",controller.front_block.controller_ptr-controller.controller_block.controller_start_ptr);
#endif
			if (to_allocate_blocks < front_borrowed_blocks_count)
			{
				front_borrowed_blocks_count = to_allocate_blocks;
				to_allocate_blocks = 0u;
			}
			else
			{
				to_allocate_blocks -= front_borrowed_blocks_count;
			}
			auto controller_start_reserved_ptr{
				controller.controller_block.controller_start_reserved_ptr};
			auto pos{
				controller.controller_block.controller_after_reserved_ptr};
			pos = ::fast_io::freestanding::non_overlapped_copy_n(controller_start_reserved_ptr,
																 front_borrowed_blocks_count,
																 pos);
			controller.controller_block.controller_start_reserved_ptr =
				controller_start_reserved_ptr + front_borrowed_blocks_count;
#if 0
			::fast_io::io::debug_perrln(::std::source_location::current(),"\n"
				"pos-controller.controller_block.controller_start_ptr=",
				pos-controller.controller_block.controller_start_ptr,"\n"
				"pos + to_allocate_blocks - controller.controller_block.controller_start_ptr=",
				pos + to_allocate_blocks - controller.controller_block.controller_start_ptr,"\n"
				"controller.controller_block.controller_after_ptr-controller.controller_block.controller_start_ptr=",
				controller.controller_block.controller_after_ptr-controller.controller_block.controller_start_ptr,"\n"
				"to_allocate_blocks=",to_allocate_blocks);
#endif
#if __cpp_if_consteval >= 202106L
			if consteval
			{
				for (auto e{pos + to_allocate_blocks}; pos != e; ++pos)
				{
					::std::construct_at(pos, static_cast<begin_ptrtype>(
#if FAST_IO_HAS_BUILTIN(__builtin_operator_new)
												 __builtin_operator_new(blockbytes * sizeof(replacetype))
#else
												 ::operator new(blockbytes * sizeof(replacetype))
#endif
													 ));
				}
			}
			else
#endif
			{
				for (auto e{pos + to_allocate_blocks}; pos != e; ++pos)
				{
					::std::construct_at(pos, static_cast<begin_ptrtype>(allocator::allocate_aligned(align, blockbytes * sizeof(replacetype))));
				}
			}
			::std::construct_at(pos, nullptr);
			controller.controller_block.controller_after_reserved_ptr = pos;
		}
	}
	if (controller.back_block.controller_ptr == controller.front_block.controller_ptr && controller.front_block.curr_ptr == controller.front_end_ptr)
	{
		auto front_block_controller_ptr{controller.front_block.controller_ptr + 1};
		controller.front_block.controller_ptr = front_block_controller_ptr;
		auto front_begin_ptr = static_cast<begin_ptrtype>(*front_block_controller_ptr);
		controller.front_block.curr_ptr = controller.front_block.begin_ptr = front_begin_ptr;
		controller.front_end_ptr = front_begin_ptr + blockbytes;
	}
}

template <typename allocator, typename dequecontroltype>
inline constexpr bool deque_reserve_back_blocks_impl(dequecontroltype &controller, ::std::size_t nb, ::std::size_t align, ::std::size_t blockbytes, ::std::size_t startoffset) noexcept
{
	if (controller.controller_block.controller_start_ptr == nullptr)
	{
		::fast_io::containers::details::deque_allocate_on_empty_common_with_n_impl<allocator>(
			controller, nb, align, blockbytes);
		auto controllerptr{controller.controller_block.controller_start_reserved_ptr};
		controller.front_block.controller_ptr = controller.back_block.controller_ptr = controllerptr;
		auto begin_ptr{*controllerptr};
		controller.front_block.begin_ptr = controller.back_block.begin_ptr = begin_ptr;
		controller.front_block.curr_ptr = controller.back_block.curr_ptr = begin_ptr + startoffset;
		controller.front_end_ptr = controller.back_end_ptr = begin_ptr + blockbytes;

		return false;
	}
	::fast_io::containers::details::deque_reserve_back_blocks_impl_none_empty<allocator>(controller, nb, align, blockbytes);
	return true;
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_grow_back_common_impl(
	dequecontroltype &controller,
	std::size_t align,
	std::size_t bytes) noexcept
{
	if (controller.controller_block.controller_start_ptr == nullptr)
	{
		::fast_io::containers::details::deque_allocate_empty_single_block_impl<allocator>(controller, align, bytes);
		return;
	}
	::fast_io::containers::details::deque_reserve_back_blocks_impl_none_empty<allocator>(controller, 1u, align, bytes);
	++controller.back_block.controller_ptr;
	auto begin_ptr{*controller.back_block.controller_ptr};
	controller.back_block.begin_ptr = begin_ptr;
	controller.back_block.curr_ptr = begin_ptr;
	controller.back_end_ptr = begin_ptr + bytes;
}

template <typename allocator, ::std::size_t align, ::std::size_t sz, ::std::size_t block_size, typename dequecontroltype>
inline constexpr void deque_grow_back_common(dequecontroltype &controller) noexcept
{
	constexpr ::std::size_t blockbytes{sz * block_size};
	::fast_io::containers::details::deque_grow_back_common_impl<allocator>(controller, align, blockbytes);
}

template <typename allocator, bool divsz, typename dequecontroltype>
inline constexpr void deque_reserve_back_spaces_impl(dequecontroltype &controller, ::std::size_t n, ::std::size_t align, ::std::size_t sz, ::std::size_t block_size)
{
	if (!n)
	{
		return;
	}
	auto back_curr_ptr{controller.back_block.curr_ptr};
	::std::size_t blocksn{static_cast<::std::size_t>(controller.back_end_ptr - back_curr_ptr)};
	if constexpr (divsz)
	{
		blocksn /= sz;
	}
	if (n <= blocksn)
	{
		return;
	}
	::std::size_t nmblocksn{n - blocksn};
	::std::size_t back_more_blocks{nmblocksn / block_size};
	::std::size_t toallocate{back_more_blocks};
#if defined(__GNUC__) || defined(__clang__)
	if constexpr (true)
	{
		if (__builtin_add_overflow(toallocate, 1u, __builtin_addressof(toallocate)))
		{
			::fast_io::fast_terminate();
		}
	}
	else
#endif
	{
		constexpr ::std::size_t mxval{::std::numeric_limits<::std::size_t>::max()};
		if (toallocate == mxval)
		{
			::fast_io::fast_terminate();
		}
		++toallocate;
	}

	if consteval
	{
		::fast_io::containers::details::deque_reserve_back_blocks_impl<allocator>(controller,
																				  toallocate, align, block_size, 1);
	}
	else
	{
		::std::size_t const block_bytes{block_size * sz};
		::fast_io::containers::details::deque_reserve_back_blocks_impl<allocator>(*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(
																					  __builtin_addressof(controller)),
																				  toallocate, align, block_bytes, sz);
	}
}

template <typename allocator, ::std::size_t align, ::std::size_t sz, ::std::size_t block_size, typename dequecontroltype>
inline constexpr void deque_reserve_back_spaces(dequecontroltype &controller, ::std::size_t n)
{
	::fast_io::containers::details::deque_reserve_back_spaces_impl<allocator, false>(controller, n, align, sz, block_size);
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_reserve_front_blocks_none_empty_impl(dequecontroltype &controller, ::std::size_t nb, ::std::size_t align, ::std::size_t blockbytes) noexcept
{
	using replacetype = typename dequecontroltype::replacetype;
	using begin_ptrtype = replacetype *;

	::std::size_t diff_to_start_ptr =
		static_cast<::std::size_t>(
			controller.front_block.controller_ptr -
			controller.controller_block.controller_start_reserved_ptr);
	if (diff_to_start_ptr < nb)
	{
		::std::size_t distance_front_to_start{
			static_cast<::std::size_t>(controller.front_block.controller_ptr -
									   controller.controller_block.controller_start_ptr)};
		if (distance_front_to_start < nb)
		{
			::fast_io::containers::details::deque_rebalance_or_grow_insertation_direction_impl<allocator>(controller, nb, false);
		}
		::std::size_t diff_to_start_ptr2 =
			static_cast<::std::size_t>(
				controller.front_block.controller_ptr -
				controller.controller_block.controller_start_reserved_ptr);
		if (diff_to_start_ptr2 < nb)
		{
			::std::size_t back_reserved_blocks{
				static_cast<::std::size_t>(controller.controller_block.controller_after_reserved_ptr -
										   controller.back_block.controller_ptr - 1)};

			::std::size_t back_borrowed_blocks_count{back_reserved_blocks};
			::std::size_t to_allocate_blocks{static_cast<::std::size_t>(nb - diff_to_start_ptr2)};
			if (to_allocate_blocks < back_reserved_blocks)
			{
				back_borrowed_blocks_count = nb;
				to_allocate_blocks = 0u;
			}
			else
			{
				to_allocate_blocks -= back_borrowed_blocks_count;
			}

			auto controller_after_reserved_ptr{
				controller.controller_block.controller_after_reserved_ptr};

			auto controller_start_reserved_ptr{
				controller.controller_block.controller_start_reserved_ptr};

			auto new_controller_start_reserved_ptr{
				controller_start_reserved_ptr - back_borrowed_blocks_count};
			auto new_controller_after_reserved_ptr{
				controller_after_reserved_ptr - back_borrowed_blocks_count};
			::fast_io::freestanding::non_overlapped_copy_n(new_controller_after_reserved_ptr,
														   back_borrowed_blocks_count,
														   new_controller_start_reserved_ptr);
			::std::construct_at(controller.controller_block.controller_after_reserved_ptr = new_controller_after_reserved_ptr, nullptr);
			// after this line Todo

			auto ed{new_controller_start_reserved_ptr};
			new_controller_start_reserved_ptr -= to_allocate_blocks;
#if __cpp_if_consteval >= 202106L
			if consteval
			{
				for (auto i{new_controller_start_reserved_ptr}; i != ed; ++i)
				{
					::std::construct_at(i, static_cast<begin_ptrtype>(
#if FAST_IO_HAS_BUILTIN(__builtin_operator_new)
											   __builtin_operator_new(blockbytes * sizeof(replacetype))
#else
											   ::operator new(blockbytes * sizeof(replacetype))
#endif
												   ));
				}
			}
			else
#endif
			{
				for (auto i{new_controller_start_reserved_ptr}; i != ed; ++i)
				{
					::std::construct_at(i, static_cast<begin_ptrtype>(allocator::allocate_aligned(align, blockbytes * sizeof(replacetype))));
				}
			}
			controller.controller_block.controller_start_reserved_ptr = new_controller_start_reserved_ptr;
		}
	}
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_reserve_front_blocks_impl(dequecontroltype &controller, ::std::size_t nb, ::std::size_t align, ::std::size_t blockbytes, ::std::size_t startoffset) noexcept
{
	if (controller.controller_block.controller_start_ptr == nullptr)
	{
		::fast_io::containers::details::deque_allocate_on_empty_common_with_n_impl<allocator>(
			controller, nb, align, blockbytes);
		auto controllerptr{controller.controller_block.controller_after_reserved_ptr - 1};
		controller.front_block.controller_ptr = controller.back_block.controller_ptr = controllerptr;
		auto begin_ptr{*controllerptr};
		controller.front_block.begin_ptr = controller.back_block.begin_ptr = begin_ptr;
		controller.front_block.curr_ptr = controller.back_block.curr_ptr = begin_ptr + (blockbytes - startoffset);
		controller.front_end_ptr = controller.back_end_ptr = begin_ptr + blockbytes;
		return;
	}
	::fast_io::containers::details::deque_reserve_front_blocks_none_empty_impl<allocator>(controller, nb, align, blockbytes);
	return;
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_reserve_front_spaces_impl(dequecontroltype &controller, ::std::size_t n, ::std::size_t align, ::std::size_t sz, ::std::size_t block_size)
{

	if (!n)
	{
		return;
	}
	auto front_curr_ptr{controller.front_block.curr_ptr};
	::std::size_t blocksn{static_cast<::std::size_t>(front_curr_ptr - controller.front_block.begin_ptr)};
	if (n <= blocksn)
	{
		return;
	}
	::std::size_t nmblocksn{n - blocksn};
	::std::size_t front_more_blocks{nmblocksn / block_size};
	::std::size_t toallocate{front_more_blocks};
#if defined(__GNUC__) || defined(__clang__)
	if constexpr (true)
	{
		if (__builtin_add_overflow(toallocate, 1u, __builtin_addressof(toallocate)))
		{
			::fast_io::fast_terminate();
		}
	}
	else
#endif
	{
		constexpr ::std::size_t mxval{::std::numeric_limits<::std::size_t>::max()};
		if (toallocate == mxval)
		{
			::fast_io::fast_terminate();
		}
		++toallocate;
	}
	if consteval
	{
		::fast_io::containers::details::deque_reserve_front_blocks_impl<allocator>(controller, toallocate, align, block_size, 1u);
	}
	else
	{
		::std::size_t const block_bytes{block_size * sz};
		::fast_io::containers::details::deque_reserve_front_blocks_impl<allocator>(*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(
																					   __builtin_addressof(controller)),
																				   toallocate, align, block_bytes, sz);
	}
}

template <typename allocator, ::std::size_t align, ::std::size_t sz, ::std::size_t block_size, typename dequecontroltype>
inline constexpr void deque_reserve_front_spaces(dequecontroltype &controller, ::std::size_t n)
{
	::fast_io::containers::details::deque_reserve_front_spaces_impl<allocator>(controller, n, align, sz, block_size);
}

template <typename allocator, typename dequecontroltype>
inline constexpr void deque_grow_front_common_impl(
	dequecontroltype &controller,
	std::size_t align,
	std::size_t bytes) noexcept
{
	if (controller.controller_block.controller_start_ptr == nullptr)
	{
		::fast_io::containers::details::deque_allocate_empty_single_block_impl<allocator>(controller, align, bytes);
		return;
	}
	::fast_io::containers::details::deque_reserve_front_blocks_none_empty_impl<allocator>(controller, 1zu, align, bytes);
	auto begin_ptr{*(--controller.front_block.controller_ptr)};
	controller.front_block.begin_ptr = begin_ptr;
	auto end_ptr{begin_ptr + bytes};
	controller.front_block.curr_ptr = end_ptr;
	controller.front_end_ptr = end_ptr;
}

template <typename allocator, ::std::size_t align, ::std::size_t sz, ::std::size_t block_size, typename dequecontroltype>
inline constexpr void deque_grow_front_common(dequecontroltype &controller) noexcept
{
	constexpr ::std::size_t blockbytes{sz * block_size};
	::fast_io::containers::details::deque_grow_front_common_impl<allocator>(controller, align, blockbytes);
}

template <typename dequecontroltype>
inline constexpr ::std::size_t deque_get_front_capacity_bytes_impl(dequecontroltype const &controller, ::std::size_t block_bytes) noexcept
{
	return block_bytes * static_cast<::std::size_t>(controller.back_block.controller_ptr - controller.controller_block.controller_start_reserved_ptr) +
		   static_cast<::std::size_t>(controller.back_block.curr_ptr - controller.back_block.begin_ptr);
}

template <::std::size_t sz, ::std::size_t block_bytes, typename dequecontroltype>
inline constexpr ::std::size_t deque_get_front_capacity(dequecontroltype const &controller) noexcept
{
	auto ret{::fast_io::containers::details::deque_get_front_capacity_bytes_impl(controller, block_bytes)};
	if constexpr (sz != 1u)
	{
		ret /= sz;
	}
	return ret;
}

template <typename dequecontroltype>
inline constexpr ::std::size_t deque_get_back_capacity_bytes_impl(dequecontroltype const &controller, ::std::size_t block_bytes) noexcept
{
	return block_bytes * static_cast<::std::size_t>(controller.controller_block.controller_after_reserved_ptr - controller.front_block.controller_ptr) -
		   static_cast<::std::size_t>(controller.front_block.curr_ptr - controller.front_block.begin_ptr);
}

template <::std::size_t sz, ::std::size_t block_bytes, typename dequecontroltype>
inline constexpr ::std::size_t deque_get_back_capacity(dequecontroltype const &controller) noexcept
{
	auto ret{::fast_io::containers::details::deque_get_back_capacity_bytes_impl(controller, block_bytes)};
	if constexpr (sz != 1u)
	{
		ret /= sz;
	}
	return ret;
}

template <typename dequecontroltype>
inline constexpr ::std::size_t deque_get_capacity_bytes_impl(dequecontroltype &controller, ::std::size_t block_bytes) noexcept
{
	return block_bytes * static_cast<::std::size_t>(controller.controller_block.controller_after_reserved_ptr - controller.controller_block.controller_start_reserved_ptr);
}

template <::std::size_t sz, ::std::size_t block_bytes, typename dequecontroltype>
inline constexpr ::std::size_t deque_get_capacity(dequecontroltype &controller) noexcept
{
	auto ret{::fast_io::containers::details::deque_get_capacity_bytes_impl(controller, block_bytes)};
	if constexpr (sz != 1u)
	{
		ret /= sz;
	}
	return ret;
}

template <typename>
struct is_fast_io_deque_iterator_impl : ::std::false_type
{};

template <typename U>
struct is_fast_io_deque_iterator_impl<
	::fast_io::containers::details::deque_iterator<U, false>> : ::std::true_type
{};

template <typename U>
struct is_fast_io_deque_iterator_impl<
	::fast_io::containers::details::deque_iterator<U, true>> : ::std::true_type
{};

template <typename T>
concept is_fast_io_deque_iterator_v =
	::fast_io::containers::details::is_fast_io_deque_iterator_impl<std::remove_cvref_t<T>>::value;

template <typename Iter>
	requires(::fast_io::containers::details::is_fast_io_deque_iterator_v<Iter>)
inline constexpr Iter uninitialized_relocate_define(
	::fast_io::operations::defines::memory_algorithm_define_type<Iter, Iter, Iter>,
	Iter first, Iter last, Iter dest)
{
	using valtype = ::std::iter_value_t<Iter>;
	if constexpr (::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<valtype>)
	{
		if !consteval
		{
			return ::std::bit_cast<Iter>(
				::fast_io::containers::details::deque_copy_impl(
					::std::bit_cast<::fast_io::containers::details::deque_control_block_common>(first.itercontent),
					::std::bit_cast<::fast_io::containers::details::deque_control_block_common>(last.itercontent),
					::std::bit_cast<::fast_io::containers::details::deque_control_block_common>(dest.itercontent),
					::fast_io::containers::details::deque_block_bytes<sizeof(valtype)>));
		}
	}
	return ::std::bit_cast<Iter>(
		::fast_io::containers::details::deque_relocate_impl(first.itercontent, last.itercontent, dest.itercontent,
															::fast_io::containers::details::deque_block_size<sizeof(valtype)>));
}

template <typename Iter>
	requires(::fast_io::containers::details::is_fast_io_deque_iterator_v<Iter>)
inline constexpr Iter uninitialized_relocate_backward_define(
	::fast_io::operations::defines::memory_algorithm_define_type<Iter, Iter, Iter>,
	Iter first, Iter last, Iter dest)
{
	using valtype = ::std::iter_value_t<Iter>;
	if constexpr (::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<valtype>)
	{
		if !consteval
		{
			return ::std::bit_cast<Iter>(
				::fast_io::containers::details::deque_copy_backward_impl(
					::std::bit_cast<::fast_io::containers::details::deque_control_block_common>(first.itercontent),
					::std::bit_cast<::fast_io::containers::details::deque_control_block_common>(last.itercontent),
					::std::bit_cast<::fast_io::containers::details::deque_control_block_common>(dest.itercontent),
					::fast_io::containers::details::deque_block_bytes<sizeof(valtype)>));
		}
	}
	return ::std::bit_cast<Iter>(
		::fast_io::containers::details::deque_relocate_backward_impl(first.itercontent, last.itercontent, dest.itercontent,
																	 ::fast_io::containers::details::deque_block_size<sizeof(valtype)>));
}

template <typename ptrtype>
struct
#if __has_cpp_attribute(__gnu__::__may_alias__)
	[[__gnu__::__may_alias__]]
#endif
	deque_nth_element_result
{
	ptrtype start_ptr, end_ptr;
};

template <typename dequecontroltype>
inline constexpr ::fast_io::containers::details::deque_nth_element_result<typename dequecontroltype::controlreplacetype> deque_nth_element_common_impl(dequecontroltype &controller, ::std::size_t pos, ::std::size_t block_size) noexcept
{
	auto front_controller_ptr{controller.front_block.controller_ptr};
	auto back_controller_ptr{controller.back_block.controller_ptr};

	::std::size_t lastsegidx{static_cast<::std::size_t>(static_cast<::std::size_t>(back_controller_ptr - front_controller_ptr))};
	if (lastsegidx < pos) [[unlikely]]
	{
		::fast_io::fast_terminate();
#if __has_cpp_attribute(unreachable)
		[[unreachable]];
#endif
	}
	using pointer = typename dequecontroltype::controlreplacetype;
	pointer start_ptr, end_ptr;
	if (!pos)
	{
		start_ptr = controller.front_block.curr_ptr;
		if (lastsegidx)
		{
			end_ptr = controller.front_end_ptr;
		}
		else
		{
			end_ptr = controller.back_block.curr_ptr;
		}
	}
	else if (pos == lastsegidx)
	{
		start_ptr = controller.back_block.begin_ptr;
		end_ptr = controller.back_block.curr_ptr;
	}
	else
	{
		start_ptr = front_controller_ptr[pos];
		end_ptr = start_ptr + block_size;
	}

	return {start_ptr, end_ptr};
}

template <::std::size_t block_size, typename dequecontroltype>
inline constexpr ::fast_io::containers::details::deque_nth_element_result<typename dequecontroltype::controlreplacetype> deque_nth_element_common(dequecontroltype &controller, ::std::size_t pos) noexcept
{
	return ::fast_io::containers::details::deque_nth_element_common_impl(controller, pos, block_size);
}

template <typename T>
inline constexpr ::std::size_t deque_itercontent_diff_impl(T const &a, T const &b, ::std::size_t block_size)
{
	::std::size_t controllerdiff{static_cast<::std::size_t>(a.controller_ptr - b.controller_ptr)};
	return controllerdiff * block_size + static_cast<::std::size_t>((a.curr_ptr - a.begin_ptr) + (b.begin_ptr - b.curr_ptr));
}

template <typename allocator, bool divsz, typename dequecontroltype>
inline constexpr void deque_reserve_back_impl(dequecontroltype &controller, ::std::size_t newbackcap, ::std::size_t align, ::std::size_t sz, ::std::size_t block_size) noexcept
{
	::std::size_t deqsz;

	if constexpr (divsz)
	{
		deqsz = ::fast_io::containers::details::deque_itercontent_diff_impl(controller.back_block, controller.front_block, block_size * sz) / sz;
	}
	else
	{
		deqsz = ::fast_io::containers::details::deque_itercontent_diff_impl(controller.back_block, controller.front_block, block_size);
	}

	::std::size_t toaddedsz;
#if defined(__GNUC__) || defined(__clang__)
	if constexpr (true)
	{
		if (__builtin_sub_overflow(newbackcap, deqsz, __builtin_addressof(toaddedsz)))
		{
			return;
		}
	}
	else
#endif
	{
		if (newbackcap <= deqsz) [[unlikely]]
		{
			return;
		}
		toaddedsz = static_cast<::std::size_t>(newbackcap - deqsz);
	}
	if constexpr (divsz)
	{
		::fast_io::containers::details::deque_reserve_back_spaces_impl<allocator, divsz>(controller, toaddedsz, align, sz, block_size);
	}
	else
	{
		::fast_io::containers::details::deque_reserve_back_spaces_impl<allocator, divsz>(controller, toaddedsz, align, 1zu, block_size * sz);
	}
}

template <typename allocator, ::std::size_t align, ::std::size_t sz, ::std::size_t block_size, bool divsz, typename dequecontroltype>
inline constexpr void deque_reserve_back_common(dequecontroltype &controller, ::std::size_t newbackcap) noexcept
{
	::fast_io::containers::details::deque_reserve_back_impl<allocator, divsz>(controller, newbackcap, align, sz, block_size);
}

template <typename allocator, bool divsz, typename dequecontroltype>
inline constexpr void deque_reserve_front_impl(dequecontroltype &controller, ::std::size_t newfrontcap, ::std::size_t align, ::std::size_t sz, ::std::size_t block_size) noexcept
{
	::std::size_t deqsz;

	if constexpr (divsz)
	{
		deqsz = ::fast_io::containers::details::deque_itercontent_diff_impl(controller.back_block, controller.front_block, block_size * sz) / sz;
	}
	else
	{
		deqsz = ::fast_io::containers::details::deque_itercontent_diff_impl(controller.back_block, controller.front_block, block_size);
	}

	::std::size_t toaddedsz;
#if defined(__GNUC__) || defined(__clang__)
	if constexpr (true)
	{
		if (__builtin_sub_overflow(newfrontcap, deqsz, __builtin_addressof(toaddedsz)))
		{
			return;
		}
	}
	else
#endif
	{
		if (newfrontcap <= deqsz) [[unlikely]]
		{
			return;
		}
		toaddedsz = static_cast<::std::size_t>(newfrontcap - deqsz);
	}
	if constexpr (divsz)
	{
		::fast_io::containers::details::deque_reserve_front_spaces_impl<allocator>(controller, toaddedsz, align, sz, block_size);
	}
	else
	{
		::fast_io::containers::details::deque_reserve_front_spaces_impl<allocator>(controller, toaddedsz, align, 1zu, block_size * sz);
	}
}

template <typename allocator, ::std::size_t align, ::std::size_t sz, ::std::size_t block_size, bool divsz, typename dequecontroltype>
inline constexpr void deque_reserve_front_common(dequecontroltype &controller, ::std::size_t newfrontcap) noexcept
{
	::fast_io::containers::details::deque_reserve_front_impl<allocator, divsz>(controller, newfrontcap, align, sz, block_size);
}

template <typename T>
inline constexpr ::std::size_t deque_itercontent_difference_unsigned_common(T const &a, T const &b, ::std::size_t blocksizedf) noexcept
{
	::std::size_t controllerdiff{static_cast<::std::size_t>(a.controller_ptr - b.controller_ptr)};
	return controllerdiff * blocksizedf + static_cast<::std::size_t>((a.curr_ptr - a.begin_ptr) + (b.begin_ptr - b.curr_ptr));
}
#if 0
template <typename allocator, bool divsz, bool zeroing, typename dequecontroltype>
inline constexpr void deque_resize_common_impl(dequecontroltype &controller, ::std::size_t newsize,
	::std::size_t align, ::std::size_t sz, ::std::size_t block_size) noexcept
{
	::std::size_t blockbytes{block_size * sz};
	size_type oldsz{
		::fast_io::containers::details::deque_itercontent_difference_unsigned_common(controller.back_block, controller.front_block, blockbytes);
	};
	if constexpr(divsz)
	{
		oldsz/=sz;
	}
	if (count == oldsz)
	{
		return;
	}
	iterator newed;
	if (count < oldsz)
	{
		auto ed{};
		newed = ed;
		newed -= static_cast<size_type>(oldsz - count);
	}
	else
	{
		this->reserve_back(count);
		auto ed{this->end()};
		newed = ed + static_cast<size_type>(count - oldsz);

	}
	if (newed.curr_ptr == newed.begin_ptr)
	{
		newed.curr_ptr = (newed.begin_ptr = *--newed.controller_ptr) + block_size;
	}
	controller.back_block.controller_ptr = newed.controller_ptr;
	controller.back_end_ptr = (this->controller.back_block.begin_ptr = newed.begin_ptr) + block_size;
	controller.back_block.curr_ptr = newed.curr_ptr;
}

template <typename allocator, ::std::size_t align, ::std::size_t sz, ::std::size_t block_size, bool divsz, bool zeroing, typename dequecontroltype>
inline constexpr void deque_resize_common(dequecontroltype &controller, ::std::size_t newsize) noexcept
{
	::fast_io::containers::details::deque_resize_common_impl<allocator, divsz, zeroing>(controller, newsize, align, sz, block_size);
}
#endif
} // namespace details

template <::std::forward_iterator ForwardIt>
inline constexpr ForwardIt rotate_for_fast_io_deque(ForwardIt first, ForwardIt middle, ForwardIt last) noexcept
{
	return ::std::rotate(first, middle, last);
}

template <typename T, typename allocator>
class deque
{
public:
	using value_type = T;
	using pointer = value_type *;
	using const_pointer = value_type const *;
	using reference = value_type &;
	using const_reference = value_type const &;
	using size_type = ::std::size_t;
	using difference_type = ::std::ptrdiff_t;
	using iterator = ::fast_io::containers::details::deque_iterator<T, false>;
	using const_iterator = ::fast_io::containers::details::deque_iterator<T, true>;
	using reverse_iterator = ::std::reverse_iterator<iterator>;
	using const_reverse_iterator = ::std::reverse_iterator<const_iterator>;

private:
	using controller_type = ::fast_io::containers::details::deque_controller<T>;

public:
	controller_type controller;
	static inline constexpr size_type block_size{::fast_io::containers::details::deque_block_size<sizeof(value_type)>};
	inline explicit constexpr deque() noexcept
		: controller{}
	{}

	inline constexpr deque(deque const &other) noexcept(::std::is_nothrow_copy_constructible_v<value_type>)
	{
		this->copy_construct_impl(other.controller);
	}
	inline constexpr deque &operator=(deque const &other) noexcept(::std::is_nothrow_copy_constructible_v<value_type>)
	{
		if (__builtin_addressof(other) == this)
		{
			return *this;
		}
#if 0
		if constexpr (::std::is_nothrow_copy_constructible_v<value_type> &&
			::std::is_nothrow_copy_assignable_v<value_type>)
		{
// Path A: High-performance reuse of existing blocks
			this->assign_range(other);
		}
		else
#endif
		{
			// Path B: Strong Exception Guarantee via Creating a Temporary
			deque temp(other);
			destroy_deque_controller(this->controller);
			this->controller = temp.controller;
			temp.controller = {};
			return *this;
		}
	}

	inline constexpr deque(deque &&other) noexcept : controller(other.controller)
	{
		other.controller = {};
	}
	inline constexpr deque &operator=(deque &&other) noexcept
	{
		if (__builtin_addressof(other) == this)
		{
			return *this;
		}
		destroy_deque_controller(this->controller);
		this->controller = other.controller;
		other.controller = {};
		return *this;
	}

private:
	struct run_destroy
	{
		controller_type *thiscontroller{};
		inline constexpr run_destroy() noexcept = default;
		inline explicit constexpr run_destroy(controller_type *p) noexcept
			: thiscontroller(p)
		{}
		inline run_destroy(run_destroy const &) = delete;
		inline run_destroy &operator=(run_destroy const &) = delete;
		inline constexpr ~run_destroy()
		{
			if (thiscontroller)
			{
				destroy_deque_controller(*thiscontroller);
			}
		}
	};
	inline constexpr void copy_construct_impl(controller_type const &fromcontroller)
	{
		if constexpr (::std::is_trivially_copyable_v<value_type>)
		{
			if (__builtin_is_constant_evaluated())
			{
				::fast_io::containers::details::deque_clone_trivial_common<allocator, alignof(value_type), 1u, block_size>(controller, fromcontroller);
			}
			else
			{
				::fast_io::containers::details::deque_clone_trivial_common<allocator, alignof(value_type), sizeof(value_type), block_size>(*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(__builtin_addressof(controller)),
																																		   *reinterpret_cast<::fast_io::containers::details::deque_controller_common const *>(__builtin_addressof(fromcontroller)));
			}
			return;
		}
		else
		{
			if (fromcontroller.front_block.curr_ptr == fromcontroller.back_block.curr_ptr)
			{
				this->controller = {};
				return;
			}

			auto front_controller_ptr{fromcontroller.front_block.controller_ptr};
			auto back_controller_ptr{fromcontroller.back_block.controller_ptr};
			::std::size_t blocks_required{static_cast<::std::size_t>(back_controller_ptr -
																	 front_controller_ptr + 1)};
			constexpr ::std::size_t block_bytes{block_size * sizeof(value_type)};
			::fast_io::containers::details::deque_allocate_init_blocks_dezeroing_impl<allocator>(controller, alignof(value_type), block_bytes, blocks_required, false);

			run_destroy destroyer(__builtin_addressof(this->controller));
			auto dq_back_backup{this->controller.back_block};
			this->controller.back_block = this->controller.front_block;
			auto dq_back_end_ptr_backup{this->controller.back_end_ptr};
			this->controller.back_end_ptr = this->controller.front_end_ptr;
			pointer lastblockbegin;
			if (front_controller_ptr == back_controller_ptr)
			{
				lastblockbegin = fromcontroller.front_block.curr_ptr;
			}
			else
			{
				auto destit{controller.front_block.controller_ptr};
				auto pos{fromcontroller.front_block.curr_ptr - fromcontroller.front_block.begin_ptr};
				::fast_io::freestanding::uninitialized_copy(
					fromcontroller.front_block.curr_ptr,
					fromcontroller.front_end_ptr,
					(controller.front_block.curr_ptr =
						 pos + controller.front_block.begin_ptr));
				this->controller.back_block.curr_ptr = controller.front_end_ptr =
					controller.front_block.begin_ptr + block_size;
				++destit;
				for (pointer *it{front_controller_ptr + 1}, *ed{back_controller_ptr}; it != ed; ++it)
				{
					pointer blockptr{*it};
					::fast_io::freestanding::uninitialized_copy_n(blockptr, block_size, *destit);
					auto new_curr_ptr{blockptr + block_size};
					this->controller.back_block = {blockptr, new_curr_ptr, destit};
					this->controller.back_end_ptr = new_curr_ptr;
					++destit;
				}
				lastblockbegin = fromcontroller.back_block.begin_ptr;
			}

			dq_back_backup.curr_ptr =
				::fast_io::freestanding::uninitialized_copy(lastblockbegin,
															fromcontroller.back_block.curr_ptr, dq_back_backup.begin_ptr);

			this->controller.back_block = dq_back_backup;
			this->controller.back_end_ptr = dq_back_end_ptr_backup;
			destroyer.thiscontroller = nullptr;
		}
	}
	inline constexpr void default_construct_impl()
	{
		run_destroy des(__builtin_addressof(this->controller));

		auto front_controller_ptr{controller.front_block.controller_ptr};
		auto back_controller_ptr{controller.back_block.controller_ptr};

		auto dq_back_backup{controller.back_block};
		controller.back_block = controller.front_block;
		auto dq_back_end_ptr_backup{controller.back_end_ptr};
		controller.back_end_ptr = controller.front_end_ptr;

		T *lastblockbegin;
		if (front_controller_ptr == back_controller_ptr)
		{
			lastblockbegin = controller.front_block.curr_ptr;
		}
		else
		{
			::fast_io::freestanding::uninitialized_default_construct(controller.front_block.curr_ptr, controller.front_end_ptr);
			this->controller.back_block.curr_ptr = this->controller.back_end_ptr;
			for (T **it{front_controller_ptr + 1}, **ed{back_controller_ptr}; it != ed; ++it)
			{
				T *blockptr{*it};
				::fast_io::freestanding::uninitialized_default_construct(blockptr, blockptr + block_size);
				auto new_curr_ptr{blockptr + block_size};
				this->controller.back_block = {blockptr, new_curr_ptr, it};
				this->controller.back_end_ptr = new_curr_ptr;
			}
			lastblockbegin = dq_back_backup.begin_ptr;
		}
		::fast_io::freestanding::uninitialized_default_construct(lastblockbegin, dq_back_backup.curr_ptr);
		this->controller.back_block = dq_back_backup;
		this->controller.back_end_ptr = dq_back_end_ptr_backup;
		des.thiscontroller = nullptr;
	}

public:
	inline explicit constexpr deque(size_type n) noexcept(::fast_io::freestanding::is_zero_default_constructible_v<value_type> ||
														  ::std::is_nothrow_default_constructible_v<value_type>)
	{
		constexpr bool iszeroconstr{::fast_io::freestanding::is_zero_default_constructible_v<value_type>};
		this->init_blocks_common(n, iszeroconstr);
		if constexpr (!iszeroconstr)
		{
			this->default_construct_impl();
		}
	}

	inline explicit constexpr deque(size_type n, ::fast_io::for_overwrite_t) noexcept(::fast_io::freestanding::is_zero_default_constructible_v<value_type> ||
																					  ::std::is_nothrow_default_constructible_v<value_type>)
	{
		if constexpr (::std::is_trivially_default_constructible_v<value_type>)
		{
			this->init_blocks_common(n, false);
		}
		else if constexpr (::fast_io::freestanding::is_zero_default_constructible_v<value_type>)
		{
			this->init_blocks_common(n, true);
		}
		else
		{
			this->init_blocks_common(n, false);
			this->default_construct_impl();
		}
	}

	template <::std::ranges::range R>
	inline explicit constexpr deque(::fast_io::freestanding::from_range_t, R &&rg)
	{
		this->construct_deque_common_impl(::std::ranges::begin(rg), ::std::ranges::end(rg));
	}

	inline explicit constexpr deque(::std::initializer_list<value_type> ilist) noexcept(::std::is_nothrow_copy_constructible_v<value_type>)
	{
		this->construct_deque_common_impl(ilist.begin(), ilist.end());
	}

	inline explicit constexpr deque(size_type n, const_reference val) noexcept(::std::is_nothrow_copy_constructible_v<value_type>)
		: controller{}
	{
		if (!n)
		{
			return;
		}
		if constexpr (::std::is_nothrow_copy_constructible_v<value_type>)
		{
			this->reserve_back(n);
			auto bg{this->begin()};
			auto ed{bg + n};
			::fast_io::freestanding::uninitialized_fill(bg, ed, val);
			this->set_newed_common(ed.itercontent);
		}
		else
		{
			deque d;
			d.reserve_back(n);
			auto bg{d.begin()};
			auto ed{bg + n};
			::fast_io::freestanding::uninitialized_fill(bg, ed, val);
			d.set_newed_common(ed.itercontent);
			this->controller = d.controller;
			d.controller = {};
		}
	}

private:
	template <typename Iter, typename Sentinel>
	inline constexpr void construct_deque_common_impl(Iter first, Sentinel last)
	{
		if (first == last)
		{
			controller = {};
			return;
		}
		run_destroy des(__builtin_addressof(this->controller));
		if constexpr (::std::sized_sentinel_for<Sentinel, Iter>)
		{
			auto const dist{::std::ranges::distance(first, last)};

			this->init_blocks_common(static_cast<::std::size_t>(dist), false);
			auto front_controller_ptr{controller.front_block.controller_ptr};
			auto back_controller_ptr{controller.back_block.controller_ptr};
			auto dq_back_backup{this->controller.back_block};
			this->controller.back_block = this->controller.front_block;
			auto dq_back_end_ptr_backup{controller.back_end_ptr};
			controller.back_end_ptr = controller.front_end_ptr;

			T *lastblockbegin;
			if (front_controller_ptr == back_controller_ptr)
			{
				lastblockbegin = controller.front_block.curr_ptr;
			}
			else
			{
				for (T **it{front_controller_ptr}, **ed{back_controller_ptr}; it != ed; ++it)
				{
					T *blockptr{*it};
					first = ::fast_io::containers::details::uninitialized_copy_n_for_deque(first, block_size, blockptr).from;
					auto new_curr_ptr{blockptr + block_size};
					this->controller.back_block = {blockptr, new_curr_ptr, it};
					this->controller.back_end_ptr = new_curr_ptr;
				}
				lastblockbegin = dq_back_backup.begin_ptr;
			}
			::fast_io::containers::details::uninitialized_copy_n_for_deque(
				first,
				static_cast<::std::size_t>(dq_back_backup.curr_ptr - lastblockbegin),
				lastblockbegin);
			this->controller.back_block = dq_back_backup;
			this->controller.back_end_ptr = dq_back_end_ptr_backup;
		}
		else
		{
			this->controller = {};
			for (; first != last; ++first)
			{
				this->push_back(*first);
			}
		}
		des.thiscontroller = nullptr;
	}

	inline constexpr void init_blocks_common(::std::size_t n, bool iszeroconstr) noexcept
	{
		if (__builtin_is_constant_evaluated())
		{
			::fast_io::containers::details::deque_init_space_common<allocator, alignof(value_type), 1u, block_size>(controller, n, iszeroconstr);
		}
		else
		{
			::fast_io::containers::details::deque_init_space_common<allocator, alignof(value_type), sizeof(value_type), block_size>(*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(__builtin_addressof(controller)), n, iszeroconstr);
		}
	}
	inline static constexpr void destroy_elements_range(
		::fast_io::containers::details::deque_control_block<value_type> const &first,
		::fast_io::containers::details::deque_control_block<value_type> const &last) noexcept
	{
		if constexpr (!::std::is_trivially_destructible_v<value_type>)
		{
			auto front_controller_ptr{first.controller_ptr};
			auto back_controller_ptr{last.controller_ptr};
			T *lastblockbegin;
			if (front_controller_ptr == back_controller_ptr)
			{
				lastblockbegin = first.curr_ptr;
			}
			else
			{
				::std::destroy(first.curr_ptr, first.begin_ptr + block_size);
				for (T **it{front_controller_ptr + 1}, **ed{back_controller_ptr}; it != ed; ++it)
				{
					T *blockptr{*it};
					::std::destroy(blockptr, blockptr + block_size);
				}
				lastblockbegin = last.begin_ptr;
			}
			::std::destroy(lastblockbegin, last.curr_ptr);
		}
	}

	inline static constexpr void destroy_all_elements(controller_type &controller) noexcept
	{
		auto front_controller_ptr{controller.front_block.controller_ptr};
		auto back_controller_ptr{controller.back_block.controller_ptr};
		T *lastblockbegin;
		if (front_controller_ptr == back_controller_ptr)
		{
			lastblockbegin = controller.front_block.curr_ptr;
		}
		else
		{
			::std::destroy(controller.front_block.curr_ptr, controller.front_end_ptr);
			for (T **it{front_controller_ptr + 1}, **ed{back_controller_ptr}; it != ed; ++it)
			{
				T *blockptr{*it};
				::std::destroy(blockptr, blockptr + block_size);
			}
			lastblockbegin = controller.back_block.begin_ptr;
		}
		::std::destroy(lastblockbegin, controller.back_block.curr_ptr);
	}

	inline static constexpr void destroy_deque_controller(controller_type &controller) noexcept
	{
		if constexpr (!::std::is_trivially_destructible_v<value_type>)
		{
			destroy_all_elements(controller);
		}
		::fast_io::containers::details::deque_destroy_trivial_common<allocator, alignof(value_type), sizeof(value_type)>(controller.controller_block);
	}

#if __has_cpp_attribute(__gnu__::__cold__)
	[[__gnu__::__cold__]]
#endif
	inline constexpr void grow_front() noexcept
	{
		if (__builtin_is_constant_evaluated())
		{
			::fast_io::containers::details::deque_grow_front_common<allocator, alignof(value_type), 1u, block_size>(controller);
		}
		else
		{
			::fast_io::containers::details::deque_grow_front_common<allocator, alignof(value_type), sizeof(value_type), block_size>(*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(__builtin_addressof(controller)));
		}
	}

#if __has_cpp_attribute(__gnu__::__cold__)
	[[__gnu__::__cold__]]
#endif
	inline constexpr void grow_back() noexcept
	{
		if (__builtin_is_constant_evaluated())
		{
			::fast_io::containers::details::deque_grow_back_common<allocator, alignof(value_type), 1u, block_size>(controller);
		}
		else
		{
			::fast_io::containers::details::deque_grow_back_common<allocator, alignof(value_type), sizeof(value_type), block_size>(*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(__builtin_addressof(controller)));
		}
	}

	inline constexpr void front_backspace() noexcept
	{
		auto front_controller_ptr{controller.front_block.controller_ptr};
		if (front_controller_ptr == controller.back_block.controller_ptr) [[unlikely]]
		{
			auto begin_ptr{controller.front_block.begin_ptr};
			constexpr ::std::size_t half_blocks_size{block_size >> 1u};
			controller.back_block.curr_ptr = (controller.front_block.curr_ptr = begin_ptr + half_blocks_size);
			return;
		}
		controller.front_end_ptr = (controller.front_block.curr_ptr = controller.front_block.begin_ptr = *(controller.front_block.controller_ptr = front_controller_ptr + 1)) + block_size;
	}

	inline constexpr void back_backspace() noexcept
	{
		auto back_controller_ptr{controller.back_block.controller_ptr};
		if (back_controller_ptr == controller.front_block.controller_ptr) [[unlikely]]
		{
			auto begin_ptr{controller.back_block.begin_ptr};
			constexpr ::std::size_t half_blocks_size{block_size >> 1u};
			controller.front_block.curr_ptr = (controller.back_block.curr_ptr = begin_ptr + half_blocks_size);
			return;
		}
		controller.back_block.curr_ptr = (controller.back_end_ptr = ((controller.back_block.begin_ptr = *(controller.back_block.controller_ptr = back_controller_ptr - 1)) + block_size));
	}

public:
	inline constexpr void clear() noexcept
	{
		if constexpr (!::std::is_trivially_destructible_v<value_type>)
		{
			destroy_all_elements(this->controller);
		}
		if (__builtin_is_constant_evaluated())
		{
			::fast_io::containers::details::deque_clear_common<allocator, 1u, block_size>(controller);
		}
		else
		{
			::fast_io::containers::details::deque_clear_common<allocator, sizeof(value_type), block_size>(*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(__builtin_addressof(controller)));
		}
	}
	template <typename... Args>
		requires ::std::constructible_from<value_type, Args...>
	inline constexpr reference emplace_back(Args &&...args) noexcept(::std::is_nothrow_constructible_v<value_type, Args...>)
	{
		if (controller.back_block.curr_ptr == controller.back_end_ptr) [[unlikely]]
		{
			grow_back();
		}
		auto currptr{controller.back_block.curr_ptr};
#if __cpp_if_consteval >= 202106L
		if consteval
		{
			if constexpr (::std::is_trivially_constructible_v<value_type, Args...>)
			{
				*currptr = value_type{::std::forward<Args>(args)...};
			}
			else
			{
				::std::construct_at(currptr, ::std::forward<Args>(args)...);
			}
		}
		else
#endif
		{
			::std::construct_at(currptr, ::std::forward<Args>(args)...);
		}
		controller.back_block.curr_ptr = currptr + 1;
		return *currptr;
	}

	inline constexpr void push_back(value_type const &value) noexcept(::std::is_nothrow_copy_constructible_v<value_type>)
	{
		this->emplace_back(value);
	}

	inline constexpr void push_back(value_type &&value) noexcept
	{
		this->emplace_back(::std::move(value));
	}
	inline constexpr void pop_back() noexcept
	{
		if (controller.front_block.curr_ptr == controller.back_block.curr_ptr) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		pop_back_unchecked();
	}

	inline constexpr void pop_back_unchecked() noexcept
	{
		if constexpr (!::std::is_trivially_destructible_v<value_type>)
		{
			::std::destroy_at(controller.back_block.curr_ptr - 1);
		}
		if (--controller.back_block.curr_ptr == controller.back_block.begin_ptr) [[unlikely]]
		{
			this->back_backspace();
		}
	}

	inline constexpr reference back_unchecked() noexcept
	{
		return controller.back_block.curr_ptr[-1];
	}

	inline constexpr const_reference back_unchecked() const noexcept
	{
		return controller.back_block.curr_ptr[-1];
	}

	inline constexpr reference back() noexcept
	{
		if (controller.front_block.curr_ptr == controller.back_block.curr_ptr) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}

		return controller.back_block.curr_ptr[-1];
	}

	inline constexpr const_reference back() const noexcept
	{
		if (controller.front_block.curr_ptr == controller.back_block.curr_ptr) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}

		return controller.back_block.curr_ptr[-1];
	}

private:
	struct emplace_front_guard
	{
		using handletype = ::fast_io::containers::details::deque_controller<value_type>;
		handletype *thisdeq;
		explicit constexpr emplace_front_guard(handletype *other) noexcept : thisdeq{other}
		{
		}
		emplace_front_guard(emplace_front_guard const &) = delete;
		emplace_front_guard &operator=(emplace_front_guard const &) = delete;
		constexpr ~emplace_front_guard()
		{
			if (this->thisdeq)
			{
				auto &frontblock{this->thisdeq->front_block};
				if (frontblock.curr_ptr ==
						this->thisdeq->front_end_ptr &&
					frontblock.controller_ptr !=
						this->thisdeq->back_block.controller_ptr)
				{
					this->thisdeq->front_end_ptr = ((frontblock.curr_ptr = frontblock.begin_ptr = *(++frontblock.controller_ptr)) + block_size);
				}
			}
		}
	};

public:
	template <typename... Args>
		requires ::std::constructible_from<value_type, Args...>
	inline constexpr reference emplace_front(Args &&...args) noexcept(::std::is_nothrow_constructible_v<value_type, Args...>)
	{
		if (controller.front_block.curr_ptr == controller.front_block.begin_ptr) [[unlikely]]
		{
			grow_front();
		}
		auto front_curr_ptr{controller.front_block.curr_ptr};
#if __cpp_if_consteval >= 202106L
		if consteval
		{
			if constexpr (::std::is_trivially_constructible_v<value_type, Args...>)
			{
				*(front_curr_ptr - 1) = value_type{::std::forward<Args>(args)...};
				controller.front_block.curr_ptr = front_curr_ptr - 1;
				return *(front_curr_ptr - 1);
			}
			else
			{
				if constexpr (::std::is_nothrow_constructible_v<value_type, Args...>)
				{
					return *(controller.front_block.curr_ptr = ::std::construct_at(front_curr_ptr - 1, ::std::forward<Args>(args)...));
				}
				else
				{
					emplace_front_guard guard(__builtin_addressof(this->controller));
					front_curr_ptr = ::std::construct_at(front_curr_ptr - 1, ::std::forward<Args>(args)...);
					guard.thisdeq = nullptr;
					return *(controller.front_block.curr_ptr = front_curr_ptr);
				}
			}
		}
		else
#endif
		{
			if constexpr (::std::is_nothrow_constructible_v<value_type, Args...>)
			{
				return *(controller.front_block.curr_ptr = ::std::construct_at(front_curr_ptr - 1, ::std::forward<Args>(args)...));
			}
			else
			{
				emplace_front_guard guard(__builtin_addressof(this->controller));
				front_curr_ptr = ::std::construct_at(front_curr_ptr - 1, ::std::forward<Args>(args)...);
				guard.thisdeq = nullptr;
				return *(controller.front_block.curr_ptr = front_curr_ptr);
			}
		}
	}

	inline constexpr void push_front(value_type const &value)
	{
		this->emplace_front(value);
	}

	inline constexpr void push_front(value_type &&value) noexcept
	{
		this->emplace_front(::std::move(value));
	}

	inline constexpr void pop_front() noexcept
	{
		if (controller.front_block.curr_ptr == controller.back_block.curr_ptr) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}

		pop_front_unchecked();
	}

	inline constexpr void pop_front_unchecked() noexcept
	{
		if constexpr (!::std::is_trivially_destructible_v<value_type>)
		{
			::std::destroy_at(controller.front_block.curr_ptr);
		}

		if (++controller.front_block.curr_ptr == controller.front_end_ptr) [[unlikely]]
		{
			this->front_backspace();
		}
	}

	inline constexpr reference front_unchecked() noexcept
	{
		return *controller.front_block.curr_ptr;
	}

	inline constexpr const_reference front_unchecked() const noexcept
	{
		return *controller.front_block.curr_ptr;
	}

	inline constexpr reference front() noexcept
	{
		if (controller.front_block.curr_ptr == controller.back_block.curr_ptr) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}

		return *controller.front_block.curr_ptr;
	}

	inline constexpr const_reference front() const noexcept
	{
		if (controller.front_block.curr_ptr == controller.back_block.curr_ptr) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}

		return *controller.front_block.curr_ptr;
	}

	inline constexpr reference operator[](size_type index) noexcept
	{
		if (size() <= index) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return ::fast_io::containers::details::deque_index_container_unsigned(controller.front_block, index);
	}

	inline constexpr const_reference operator[](size_type index) const noexcept
	{
		if (size() <= index) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return ::fast_io::containers::details::deque_index_container_unsigned(controller.front_block, index);
	}

	inline constexpr reference index_unchecked(size_type index) noexcept
	{
		return ::fast_io::containers::details::deque_index_container_unsigned(controller.front_block, index);
	}

	inline constexpr const_reference index_unchecked(size_type index) const noexcept
	{
		return ::fast_io::containers::details::deque_index_container_unsigned(controller.front_block, index);
	}

	static inline constexpr size_type max_size() noexcept
	{
		constexpr size_type mxval{::std::numeric_limits<::std::size_t>::max() / sizeof(value_type)};
		return mxval;
	}

	static inline constexpr size_type max_size_bytes() noexcept
	{
		constexpr size_type mxval{::std::numeric_limits<::std::size_t>::max() / sizeof(value_type) * sizeof(value_type)};
		return mxval;
	}

	inline constexpr size_type size() const noexcept
	{
		return block_size * static_cast<size_type>(controller.back_block.controller_ptr - controller.front_block.controller_ptr) + static_cast<size_type>((controller.back_block.curr_ptr - controller.back_block.begin_ptr) + (controller.front_block.begin_ptr - controller.front_block.curr_ptr));
	}

	inline constexpr size_type size_bytes() const noexcept
	{
		return size() * sizeof(value_type);
	}

	inline constexpr size_type front_capacity_bytes() const noexcept
	{
		if consteval
		{
			return ::fast_io::containers::details::deque_get_front_capacity<1u, block_size>(this->controller) * sizeof(value_type);
		}
		else
		{
			return ::fast_io::containers::details::deque_get_front_capacity<1u, block_size * sizeof(value_type)>(
				*reinterpret_cast<::fast_io::containers::details::deque_controller_common const *>(this->controller));
		}
	}

	inline constexpr size_type front_capacity() const noexcept
	{
		if consteval
		{
			return ::fast_io::containers::details::deque_get_front_capacity<1u, block_size>(this->controller);
		}
		else
		{
			return ::fast_io::containers::details::deque_get_front_capacity<sizeof(value_type), block_size * sizeof(value_type)>(
				*reinterpret_cast<::fast_io::containers::details::deque_controller_common const *>(__builtin_addressof(this->controller)));
		}
	}

	inline constexpr size_type back_capacity_bytes() const noexcept
	{
		if consteval
		{
			return ::fast_io::containers::details::deque_get_back_capacity<1u, block_size>(this->controller) * sizeof(value_type);
		}
		else
		{
			return ::fast_io::containers::details::deque_get_back_capacity<sizeof(value_type), block_size * sizeof(value_type)>(
				*reinterpret_cast<::fast_io::containers::details::deque_controller_common const *>(this->controller));
		}
	}

	inline constexpr size_type back_capacity() const noexcept
	{
		if consteval
		{
			return ::fast_io::containers::details::deque_get_back_capacity<1u, block_size>(this->controller);
		}
		else
		{
			return ::fast_io::containers::details::deque_get_back_capacity<sizeof(value_type), block_size * sizeof(value_type)>(
				*reinterpret_cast<::fast_io::containers::details::deque_controller_common const *>(__builtin_addressof(this->controller)));
		}
	}

	inline constexpr size_type capacity_bytes() const noexcept
	{
		if consteval
		{
			return ::fast_io::containers::details::deque_get_capacity<1u, block_size>(this->controller) * sizeof(value_type);
		}
		else
		{
			return ::fast_io::containers::details::deque_get_capacity<1u, block_size * sizeof(value_type)>(
				*reinterpret_cast<::fast_io::containers::details::deque_controller_common const *>(this->controller));
		}
	}

	inline constexpr size_type capacity() const noexcept
	{
		if consteval
		{
			return ::fast_io::containers::details::deque_get_capacity<1u, block_size>(this->controller);
		}
		else
		{
			return ::fast_io::containers::details::deque_get_capacity<sizeof(value_type), block_size * sizeof(value_type)>(
				*reinterpret_cast<::fast_io::containers::details::deque_controller_common const *>(__builtin_addressof(this->controller)));
		}
	}

	inline static constexpr size_type size_per_segment() noexcept
	{
		return block_size;
	}

	inline static constexpr size_type size_bytes_per_segment() noexcept
	{
		constexpr size_type sz{block_size * sizeof(value_type)};
		return sz;
	}

	inline constexpr size_type segments_count() noexcept
	{
		return static_cast<size_type>(static_cast<size_type>(this->controller.back_block.controller_ptr - this->controller.front_block.controller_ptr) + 1u);
	}

	inline constexpr ::fast_io::containers::span<value_type> nth_segment(size_type pos) noexcept
	{
		if consteval
		{
			auto [start_ptr, end_ptr] = ::fast_io::containers::details::deque_nth_element_common<block_size>(this->controller, pos);
			return ::fast_io::containers::span<value_type>(start_ptr, end_ptr);
		}
		else
		{
			auto [start_ptr, end_ptr] = ::std::bit_cast<::fast_io::containers::details::deque_nth_element_result<pointer>>(::fast_io::containers::details::deque_nth_element_common<block_size * sizeof(value_type)>(*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(__builtin_addressof(this->controller)), pos));
			return ::fast_io::containers::span<value_type>(start_ptr, end_ptr);
		}
	}
	inline constexpr ::fast_io::containers::span<value_type const> const_nth_segment(size_type pos) const noexcept
	{
		if consteval
		{
			auto [start_ptr, end_ptr] = ::fast_io::containers::details::deque_nth_element_common<block_size>(this->controller, pos);
			return ::fast_io::containers::span<value_type const>(start_ptr, end_ptr);
		}
		else
		{
			auto [start_ptr, end_ptr] = ::std::bit_cast<::fast_io::containers::details::deque_nth_element_result<const_pointer>>(::fast_io::containers::details::deque_nth_element_common<block_size * sizeof(value_type)>(*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(
																																																							   const_cast<controller_type *>(__builtin_addressof(this->controller))),
																																																						   pos));
			return ::fast_io::containers::span<value_type const>(start_ptr, end_ptr);
		}
	}
	inline constexpr ::fast_io::containers::span<value_type const> nth_segment(size_type pos) const noexcept
	{
		return this->const_nth_segment(pos);
	}

	inline constexpr iterator begin() noexcept
	{
		return {this->controller.front_block};
	}

	inline constexpr const_iterator begin() const noexcept
	{
		return {this->controller.front_block};
	}

	inline constexpr const_iterator cbegin() const noexcept
	{
		return {this->controller.front_block};
	}

	inline constexpr reverse_iterator rend() noexcept
	{
		return reverse_iterator({this->controller.front_block});
	}

	inline constexpr const_reverse_iterator rend() const noexcept
	{
		return const_reverse_iterator({this->controller.front_block});
	}

	inline constexpr const_reverse_iterator crend() const noexcept
	{
		return const_reverse_iterator({this->controller.front_block});
	}

private:
	inline constexpr ::fast_io::containers::details::deque_control_block<value_type> end_common() const noexcept
	{
		::fast_io::containers::details::deque_control_block<value_type> backblock{this->controller.back_block};
		if (backblock.curr_ptr == this->controller.back_end_ptr) [[unlikely]]
		{
			if (backblock.controller_ptr) [[likely]]
			{
				backblock.curr_ptr = backblock.begin_ptr = (*++backblock.controller_ptr);
			}
		}
		return {backblock};
	}

public:
	inline constexpr iterator end() noexcept
	{
		return {this->end_common()};
	}

	inline constexpr const_iterator end() const noexcept
	{
		return {this->end_common()};
	}

	inline constexpr const_iterator cend() const noexcept
	{
		return {this->end_common()};
	}

	inline constexpr reverse_iterator rbegin() noexcept
	{
		return reverse_iterator({this->end_common()});
	}

	inline constexpr const_reverse_iterator rbegin() const noexcept
	{
		return const_reverse_iterator({this->end_common()});
	}

	inline constexpr const_reverse_iterator crbegin() const noexcept
	{
		return const_reverse_iterator({this->end_common()});
	}

	inline constexpr bool empty() const noexcept
	{
		return controller.front_block.curr_ptr == controller.back_block.curr_ptr;
	}

	inline constexpr bool is_empty() const noexcept
	{
		return controller.front_block.curr_ptr == controller.back_block.curr_ptr;
	}

	inline constexpr size_type index_of(const_iterator it) const noexcept
	{
		return ::fast_io::containers::details::deque_iter_difference_unsigned_common(it.itercontent, this->controller.front_block);
	}

	inline constexpr size_type segment_nth_of(const_iterator it) const noexcept
	{
		auto controllerptr{it.itercontent.controller_ptr};
		if (this->controller.back_block.controller_ptr < controllerptr) [[unlikely]]
		{
			--controllerptr;
		}
		return static_cast<size_type>(controllerptr - this->controller.front_controller_ptr);
	}

	inline constexpr void clear_destroy() noexcept
	{
		destroy_deque_controller(this->controller);
		this->controller = {};
	}

private:
	struct insert_range_result
	{
		size_type pos;
		iterator it;
	};
	inline constexpr insert_range_result insert_n_front_common_impl(size_type pos, size_type rgsize) noexcept
	{
		::fast_io::containers::details::deque_reserve_front_spaces<allocator,
																   alignof(value_type), sizeof(value_type), block_size>(this->controller, rgsize);
		auto thisbg{this->begin()};
		auto posit{thisbg + pos};
		auto thisbgrgsize{thisbg - rgsize};
		auto thisbgrgsizenew{::fast_io::freestanding::uninitialized_relocate(thisbg, posit, thisbgrgsize)};
		this->controller.front_block = thisbgrgsize.itercontent;
		this->controller.front_end_ptr = thisbgrgsize.itercontent.begin_ptr + block_size;
		return {pos, thisbgrgsizenew};
	}
	template <::std::ranges::range R>
		requires ::std::constructible_from<value_type, ::std::ranges::range_value_t<R>>
	inline constexpr insert_range_result insert_range_front_impl(size_type pos, R &&rg, size_type rgsize) noexcept(::std::is_nothrow_constructible_v<value_type, ::std::ranges::range_value_t<R>>)
	{
		auto ret{this->insert_n_front_common_impl(pos, rgsize)};
		::fast_io::freestanding::uninitialized_copy_n(::std::ranges::cbegin(rg), rgsize, ret.it);
		return ret;
	}
	inline constexpr insert_range_result insert_n_back_common_impl(size_type pos, size_type rgsize) noexcept
	{
		::fast_io::containers::details::deque_reserve_back_spaces<allocator,
																  alignof(value_type), sizeof(value_type), block_size>(this->controller, rgsize);
		auto posit{this->begin() + pos};
		auto thisend{this->end()};
		auto thisendrgsize{thisend + rgsize};
		::fast_io::freestanding::uninitialized_relocate_backward(posit,
																 thisend, thisendrgsize);
		if (thisendrgsize.itercontent.begin_ptr == thisendrgsize.itercontent.curr_ptr)
		{
			thisendrgsize.itercontent.curr_ptr =
				(thisendrgsize.itercontent.begin_ptr = *--thisendrgsize.itercontent.controller_ptr) + block_size;
		}
		this->controller.back_block = thisendrgsize.itercontent;
		this->controller.back_end_ptr = thisendrgsize.itercontent.begin_ptr + block_size;
		return {pos, posit};
	}
	template <::std::ranges::range R>
		requires ::std::constructible_from<value_type, ::std::ranges::range_value_t<R>>
	inline constexpr insert_range_result insert_range_back_impl(size_type pos, R &&rg, size_type rgsize) noexcept(::std::is_nothrow_constructible_v<value_type, ::std::ranges::range_value_t<R>>)
	{
		auto ret{this->insert_n_back_common_impl(pos, rgsize)};
		::fast_io::freestanding::uninitialized_copy_n(::std::ranges::cbegin(rg), rgsize, ret.it);
		return ret;
	}
	template <::std::ranges::range R>
		requires ::std::constructible_from<value_type, ::std::ranges::range_value_t<R>>
	inline constexpr insert_range_result insert_range_impl(size_type pos, R &&rg, size_type old_size) noexcept(::std::is_nothrow_constructible_v<value_type, ::std::ranges::range_value_t<R>>)
	{
		if constexpr (::std::ranges::sized_range<R>)
		{
			size_type const rgsize{::std::ranges::size(rg)};
			if (!rgsize)
			{
				return {pos, this->begin() + pos};
			}
			size_type const half_size{old_size >> 1u};
			insert_range_result ret;
			if (pos < half_size)
			{
				ret = this->insert_n_front_common_impl(pos, rgsize);
			}
			else
			{
				ret = this->insert_n_back_common_impl(pos, rgsize);
			}
			::fast_io::freestanding::uninitialized_copy_n(::std::ranges::cbegin(rg), rgsize, ret.it);
			return ret;
		}
		else
		{
			size_type retpos;
			iterator retit, rotfirst, rotmid, rotlast;
			size_type const quarterold_size{old_size >> 2u};
			if (pos < quarterold_size)
			{
				this->prepend_range(rg);
				size_type const new_size{this->size()};
				size_type const inserted{new_size - old_size};
				auto bg{this->begin()};
				size_type newpos{pos + inserted};
				rotfirst = bg;
				rotmid = bg + inserted;
				retpos = newpos;
				retit = rotlast = bg + newpos;
			}
			else
			{
				this->append_range(rg);
				auto bg{this->begin()};
				rotfirst = retit = bg + pos;
				rotmid = bg + old_size;
				rotlast = this->end();
				retpos = pos;
			}
			::fast_io::containers::rotate_for_fast_io_deque(rotfirst, rotmid, rotlast);
			return {retpos, retit};
		}
	}

public:
	template <::std::ranges::range R>
		requires ::std::constructible_from<value_type, ::std::ranges::range_value_t<R>>
	inline constexpr iterator insert_range(const_iterator pos, R &&rg) noexcept(::std::is_nothrow_constructible_v<value_type, ::std::ranges::range_value_t<R>>)
	{
		return this->insert_range_impl(
					   ::fast_io::containers::details::deque_iter_difference_unsigned_common(pos.itercontent, this->controller.front_block), ::std::forward<R>(rg), this->size())
			.it;
	}

	template <::std::ranges::range R>
		requires ::std::constructible_from<value_type, ::std::ranges::range_value_t<R>>
	inline constexpr size_type insert_range_index(size_type pos, R &&rg) noexcept(::std::is_nothrow_constructible_v<value_type, ::std::ranges::range_value_t<R>>)
	{
		size_type const n{this->size()};
		if (n < pos) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return this->insert_range_impl(pos, ::std::forward<R>(rg), n).pos;
	}

private:
	template <bool isprepend>
	struct prepend_or_append_range_guard
	{
		deque *thisdeq{};
		size_type oldn{};
		constexpr ~prepend_or_append_range_guard()
		{
			if (thisdeq)
			{
				if constexpr (isprepend)
				{
					thisdeq->erase(thisdeq->cbegin(), thisdeq->cend() - oldn);
				}
				else
				{
					thisdeq->erase(thisdeq->cbegin() + oldn, thisdeq->cend());
				}
			}
		}
	};
	using append_range_guard = prepend_or_append_range_guard<false>;
	using prepend_range_guard = prepend_or_append_range_guard<true>;

public:
	template <::std::ranges::range R>
		requires ::std::constructible_from<value_type, ::std::ranges::range_value_t<R>>
	inline constexpr void append_range(R &&rg) noexcept(::std::is_nothrow_constructible_v<value_type, ::std::ranges::range_value_t<R>>)
	{
		if constexpr (::std::ranges::sized_range<R>)
		{
			size_type const rgsize{::std::ranges::size(rg)};
			if (!rgsize)
			{
				return;
			}
			// To do write append specific code without using insert_range
			this->insert_range_back_impl(this->size(), ::std::forward<R>(rg), rgsize);
		}
		else
		{
			// To do: cleanup code
			if constexpr (::std::is_nothrow_constructible_v<value_type, ::std::ranges::range_value_t<R>>)
			{
				for (auto &&e : rg)
				{
					this->emplace_back(::std::forward<decltype(e)>(e));
				}
			}
			else
			{
				append_range_guard guard{this, this->size()};
				for (auto &&e : rg)
				{
					this->emplace_back(::std::forward<decltype(e)>(e));
				}
				guard.thisdeq = nullptr;
			}
		}
	}
#if 0
private:
	template <::std::ranges::range R>
		requires ::std::constructible_from<value_type, ::std::ranges::range_value_t<R>>
	inline constexpr size_type prepend_range_impl(R &&rg) noexcept(::std::is_nothrow_constructible_v<value_type, ::std::ranges::range_value_t<R>>)
	{
		size_type const old_size{this->size()};
		for (auto &&e : rg)
		{
			this->emplace_front(::std::forward<decltype(e)>(e));
		}
		return this->size() - old_size;
	}
#endif
public:
	template <::std::ranges::range R>
		requires ::std::constructible_from<value_type, ::std::ranges::range_value_t<R>>
	inline constexpr void prepend_range(R &&rg) noexcept(
		::std::is_nothrow_constructible_v<value_type, ::std::ranges::range_value_t<R>> &&
		::std::is_nothrow_swappable_v<value_type>)
	{
		if constexpr (::std::ranges::sized_range<R>)
		{
			size_type const rgsize{::std::ranges::size(rg)};
			if (!rgsize)
			{
				return;
			}
			this->insert_range_front_impl(0, ::std::forward<R>(rg), rgsize);
		}
		else
		{
			// To do: cleanup code
			size_type oldn{this->size()};
			if constexpr (
				::std::is_nothrow_constructible_v<value_type, ::std::ranges::range_value_t<R>> &&
				::std::is_nothrow_swappable_v<value_type>)
			{
				for (auto &&e : rg)
				{
					this->emplace_front(::std::forward<decltype(e)>(e));
				}
				::std::reverse(this->begin(), this->end() - oldn);
			}
			else
			{
				prepend_range_guard guard{this, oldn};
				for (auto &&e : rg)
				{
					this->emplace_front(::std::forward<decltype(e)>(e));
				}
				::std::reverse(this->begin(), this->end() - oldn);
				guard.thisdeq = nullptr;
			}
		}
	}

private:
	inline constexpr insert_range_result emplace_index_n_impl(size_type pos, size_type n, size_type old_size) noexcept
	{
		size_type const half_size{old_size >> 1u};
		insert_range_result ret;
		if (pos < half_size)
		{
			ret = this->insert_n_front_common_impl(pos, n);
		}
		else
		{
			ret = this->insert_n_back_common_impl(pos, n);
		}
		return ret;
	}
	inline constexpr insert_range_result emplace_index_impl(size_type pos, size_type n) noexcept
	{
		return this->emplace_index_n_impl(pos, 1u, n);
	}
	inline constexpr insert_range_result emplace_impl(size_type pos) noexcept
	{
		return this->emplace_index_impl(pos, this->size());
	}

	struct emplace_decision
	{
		iterator pos;
		::std::int_fast8_t decision;
	};
	template <bool isnothrow>
	inline constexpr ::std::conditional_t<isnothrow,
										  iterator, emplace_decision>
	emplace_decision_common(const_iterator iter) noexcept
	{
		// eh safety for being and end.
		auto iter_curr_ptr{iter.itercontent.curr_ptr};
		if (iter_curr_ptr == this->controller.back_block.curr_ptr ||
			this->controller.back_block.controller_ptr < iter.itercontent.controller_ptr)
		{
			if (this->controller.back_block.curr_ptr != controller.back_end_ptr) [[likely]]
			{
				iterator retit{this->controller.back_block.begin_ptr,
							   this->controller.back_block.curr_ptr++,
							   this->controller.back_block.controller_ptr};
				if constexpr (isnothrow)
				{
					return retit;
				}
				else
				{
					return emplace_decision{retit, 1};
				}
			}
		}
		else if (iter_curr_ptr == this->controller.front_block.curr_ptr)
		{
			if (this->controller.front_block.curr_ptr != this->controller.front_block.begin_ptr) [[likely]]
			{
				iterator retit{this->controller.front_block.begin_ptr,
							   --this->controller.front_block.curr_ptr,
							   this->controller.front_block.controller_ptr};
				if constexpr (isnothrow)
				{
					return retit;
				}
				else
				{
					return emplace_decision{retit, -1};
				}
			}
		}
		iterator retit{this->emplace_impl(::fast_io::containers::details::deque_iter_difference_unsigned_common(iter.itercontent, this->controller.front_block)).it};
		if constexpr (isnothrow)
		{
			return retit;
		}
		else
		{
			return emplace_decision{retit, 0};
		}
	}
	struct emplace_guard
	{
		deque *thisdeq;
		iterator retit;
		::std::int_fast8_t decision;
		constexpr ~emplace_guard()
		{
			if (thisdeq) [[unlikely]]
			{
				auto &thiscontroller{thisdeq->controller};
				if (decision < 0)
				{
					++thiscontroller.front_block.curr_ptr;
				}
				else if (0 < decision)
				{
					--thiscontroller.back_block.curr_ptr;
				}
				else
				{
					::std::size_t const distofront{
						::fast_io::containers::details::deque_iter_difference_unsigned_common(retit.itercontent, thisdeq->controller.front_block)};
					::std::size_t const deqsize{thisdeq->size()};
					thisdeq->erase_unchecked_single_nodestroy_impl(retit, static_cast<::std::size_t>(deqsize - distofront) < distofront);
				}
			}
		}
	};
	struct emplace_index_decision
	{
		pointer retptr;
		::std::int_fast8_t decision;
	};
	template <bool isnothrow>
	inline constexpr ::std::conditional_t<isnothrow,
										  pointer, emplace_index_decision>
	emplace_index_decision_common(::std::size_t idx) noexcept
	{
		auto oldsize{this->size()};
		if (oldsize < idx) [[unlikely]]
		{
			::fast_io::fast_terminate();
#if __has_cpp_attribute(unreachable)
			[[unreachable]];
#endif
		}
		else if (idx == oldsize)
		{
			if (this->controller.back_block.curr_ptr != controller.back_end_ptr) [[likely]]
			{
				pointer retptr{this->controller.back_block.curr_ptr++};
				if constexpr (isnothrow)
				{
					return retptr;
				}
				else
				{
					return emplace_index_decision{retptr, 1};
				}
			}
		}
		else if (!idx)
		{
			if (this->controller.front_block.curr_ptr != this->controller.front_block.begin_ptr) [[likely]]
			{
				pointer retptr{--this->controller.front_block.curr_ptr};
				if constexpr (isnothrow)
				{
					return retptr;
				}
				else
				{
					return emplace_index_decision{retptr, -1};
				}
			}
		}
		auto result{this->emplace_index_impl(idx, oldsize)};
		pointer retptr{result.it.itercontent.curr_ptr};
		if constexpr (isnothrow)
		{
			return retptr;
		}
		else
		{
			return emplace_index_decision{retptr, 0};
		}
	}

	struct emplace_index_guard
	{
		deque *thisdeq;
		::std::size_t pos;
		::std::size_t oldsize;
		::std::int_fast8_t decision;
		constexpr ~emplace_index_guard()
		{
			if (thisdeq) [[unlikely]]
			{
				auto &thiscontroller{thisdeq->controller};
				if (decision < 0)
				{
					++thiscontroller.front_block.curr_ptr;
				}
				else if (0 < decision)
				{
					--thiscontroller.back_block.curr_ptr;
				}
				else
				{
					thisdeq->erase_unchecked_single_nodestroy_impl(
						thisdeq->begin() + static_cast<::std::ptrdiff_t>(pos),
						static_cast<::std::size_t>(oldsize - pos) <= pos);
				}
			}
		}
	};

public:
	template <typename... Args>
	inline constexpr iterator emplace(const_iterator iter, Args &&...args) noexcept(::std::is_nothrow_constructible_v<value_type, Args...>)
	{
		if constexpr (::std::is_nothrow_constructible_v<value_type, Args...>)
		{
			auto retit = this->emplace_decision_common<true>(iter);
			::std::construct_at(retit.itercontent.curr_ptr, ::std::forward<Args>(args)...);
			return retit;
		}
		else
		{
			auto [retit, decision] = this->emplace_decision_common<false>(iter);
			emplace_guard guard{this, retit, decision};
			::std::construct_at(retit.itercontent.curr_ptr, ::std::forward<Args>(args)...);
			guard.thisdeq = nullptr;
			return retit;
		}
	}
	template <typename... Args>
	inline constexpr reference emplace_index(size_type idx, Args &&...args) noexcept(::std::is_nothrow_constructible_v<value_type, Args...>)
	{
		// bounds checking && eh safety for being and end.
		auto oldsize{this->size()};
		if (oldsize < idx) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		if constexpr (::std::is_nothrow_constructible_v<value_type, Args...>)
		{
			auto retptr = this->emplace_index_decision_common<true>(idx);
			::std::construct_at(retptr, ::std::forward<Args>(args)...);
			return *retptr;
		}
		else
		{
			auto [retptr, decision] = this->emplace_index_decision_common<false>(idx);
			emplace_index_guard guard{this, idx, oldsize, decision};
			::std::construct_at(retptr, ::std::forward<Args>(args)...);
			guard.thisdeq = nullptr;
			return *retptr;
		}
	}

	inline constexpr iterator insert(const_iterator iter, const_reference val) noexcept(::std::is_nothrow_copy_constructible_v<value_type>)
	{
		return this->emplace(iter, val);
	}
	inline constexpr iterator insert(const_iterator iter, value_type &&val) noexcept(::std::is_nothrow_move_constructible_v<value_type>)
	{
		return this->emplace(iter, ::std::move(val));
	}
	inline constexpr reference insert_index(size_type idx, const_reference val) noexcept(::std::is_nothrow_copy_constructible_v<value_type>)
	{
		return this->emplace_index(idx, val);
	}
	inline constexpr reference insert_index(size_type idx, value_type &&val) noexcept(::std::is_nothrow_move_constructible_v<value_type>)
	{
		return this->emplace_index(idx, ::std::move(val));
	}


private:
	struct insert_index_guard
	{
		deque *thisdeq;
		iterator pos;
		size_type count;
		constexpr ~insert_index_guard()
		{
			if (thisdeq) [[unlikely]]
			{
				thisdeq->erase_unchecked_nodestroy_for_insert_counts_impl(pos, count);
			}
		}
	};

	inline constexpr insert_range_result insert_index_impl(size_type idx, size_type count, const_reference val, size_type oldn) noexcept(::std::is_nothrow_copy_constructible_v<value_type>)
	{
		insert_range_result res{this->emplace_index_n_impl(idx, count, oldn)};
		if constexpr (::std::is_nothrow_copy_constructible_v<value_type>)
		{
			::fast_io::freestanding::uninitialized_fill_n(res.it, count, val);
		}
		else
		{
			insert_index_guard g{this, res.it, count};
			::fast_io::freestanding::uninitialized_fill_n(res.it, count, val);
			g.thisdeq = nullptr;
		}
		return res;
	}

public:
	inline constexpr iterator insert(const_iterator iter, size_type count, const_reference val) noexcept(::std::is_nothrow_copy_constructible_v<value_type>)
	{
		return this->insert_index_impl(::fast_io::containers::details::deque_iter_difference_unsigned_common(iter.itercontent, this->controller.front_block), count, val, this->size()).it;
	}
	inline constexpr size_type insert_index(size_type idx, size_type count, const_reference val) noexcept(::std::is_nothrow_copy_constructible_v<value_type>)
	{
		size_type const n{this->size()};
		if (n < idx) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return this->insert_index_impl(idx, count, val, n).pos;
	}
	inline constexpr void shrink_to_fit() noexcept(::std::is_nothrow_move_constructible_v<value_type>)
	{
		if consteval
		{
			::fast_io::containers::details::deque_shrink_to_fit_impl<allocator, alignof(value_type), block_size * sizeof(value_type)>(this->controller);
		}
		else
		{
			::fast_io::containers::details::deque_shrink_to_fit_impl<allocator, alignof(value_type), block_size * sizeof(value_type)>(*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(__builtin_addressof(
				this->controller)));
		}
	}

public:
	/*
	"The reserve_back and reserve_front APIs are fundamentally problematic for a deque. Because our logic redistributes and borrows blocks from both ends to maintain balance, direction-specific reservation is semantically unstable. A unified reserve(n) is likely more appropriate. Boost's approach appears to share this design flaw; for deques, resize for_overwrite provides clear utility, whereas direction-based reserve is often a misnomer."
	*/
	inline constexpr void reserve_back(size_type backcap) noexcept
	{
		if consteval
		{
			::fast_io::containers::details::deque_reserve_back_common<allocator, alignof(value_type), sizeof(value_type), block_size, false>(this->controller, backcap);
		}
		else
		{
			::fast_io::containers::details::deque_reserve_back_common<allocator, alignof(value_type), sizeof(value_type), block_size, true>(*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(__builtin_addressof(this->controller)), backcap);
		}
	}
	inline constexpr void reserve_front(size_type frontcap) noexcept
	{
		if consteval
		{
			::fast_io::containers::details::deque_reserve_front_common<allocator, alignof(value_type), sizeof(value_type), block_size, false>(this->controller, frontcap);
		}
		else
		{
			::fast_io::containers::details::deque_reserve_front_common<allocator, alignof(value_type), sizeof(value_type), block_size, true>(*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(__builtin_addressof(this->controller)), frontcap);
		}
	}

	inline constexpr void set_newed_common(::fast_io::containers::details::deque_control_block<value_type> newed) noexcept
	{
		if (newed.curr_ptr == newed.begin_ptr)
		{
			if (newed.controller_ptr == this->controller.front_block.controller_ptr)
			{
				this->controller.front_block.curr_ptr = newed.curr_ptr = newed.begin_ptr + (block_size >> 1u);
			}
			else
			{
				newed.curr_ptr = (newed.begin_ptr = *--newed.controller_ptr) + block_size;
			}
		}
		this->controller.back_block.controller_ptr = newed.controller_ptr;
		this->controller.back_end_ptr = (this->controller.back_block.begin_ptr = newed.begin_ptr) + block_size;
		this->controller.back_block.curr_ptr = newed.curr_ptr;
	}

public:
	inline constexpr void assign(size_type count, const_reference val) noexcept(::std::is_nothrow_copy_constructible_v<value_type>)
	{
		if constexpr (::std::is_nothrow_copy_constructible_v<value_type>)
		{
			this->clear();
			if (!count)
			{
				return;
			}
			this->reserve_back(count);
			auto bg{this->begin()};
			auto newed{bg + count};
			::fast_io::freestanding::uninitialized_fill(bg, newed, val);
			this->set_newed_common(newed.itercontent);
		}
		else
		{
			deque d(count, val);
			this->controller = d.controller;
			d.controller = {};
		}
	}
	template <::std::ranges::range R>
		requires ::std::constructible_from<value_type, ::std::ranges::range_value_t<R>>
	inline constexpr void assign_range(R &&rg) noexcept(::std::is_nothrow_constructible_v<value_type, ::std::ranges::range_value_t<R>>)
	{
		deque temp(::fast_io::freestanding::from_range, ::std::forward<R>(rg));
		this->swap(temp);
	}

private:
	inline constexpr void resize_impl(size_type count, T const *pval) noexcept(::std::is_nothrow_move_constructible_v<value_type>)
	{
		size_type oldsz{this->size()};
		if (count == oldsz)
		{
			return;
		}
		iterator newed;

		if (count < oldsz)
		{
			auto ed{this->end()};
			newed = ed;
			newed -= static_cast<size_type>(oldsz - count);
			if constexpr (!::std::is_trivially_destructible_v<value_type>)
			{
				this->destroy_elements_range(newed.itercontent, ed.itercontent);
			}
		}
		else
		{
			this->reserve_back(count);

			auto ed{this->end()};
			newed = ed + static_cast<size_type>(count - oldsz);
			if (pval)
			{
				::fast_io::freestanding::uninitialized_fill(ed, newed, *pval);
			}
			else
			{
				::fast_io::freestanding::uninitialized_default_construct(ed, newed);
			}
		}
		this->set_newed_common(newed.itercontent);
	}
	inline constexpr void resize_for_overwrite_impl(size_type count) noexcept(::std::is_nothrow_move_constructible_v<value_type>)
	{
		size_type oldsz{this->size()};
		if (count == oldsz)
		{
			return;
		}
		iterator newed;
		if (count < oldsz)
		{
			auto ed{this->end()};
			newed = ed;
			newed -= static_cast<size_type>(oldsz - count);
			if constexpr (!::std::is_trivially_destructible_v<value_type>)
			{
				this->erase(newed, ed);
				return;
			}
		}
		else
		{
			this->reserve_back(count);
			auto ed{this->end()};
			newed = ed + static_cast<size_type>(count - oldsz);
		}
		this->set_newed_common(newed.itercontent);
	}

public:
	inline constexpr void resize(size_type count) noexcept(::std::is_nothrow_default_constructible_v<value_type> && ::std::is_nothrow_move_constructible_v<value_type>)
	{
		this->resize_impl(count, nullptr);
	}
	inline constexpr void resize(size_type count, ::fast_io::for_overwrite_t) noexcept(::fast_io::freestanding::is_zero_default_constructible_v<value_type> ||
																					   ::std::is_nothrow_default_constructible_v<value_type>)
	{
		/*
		Todo
		*/
		if constexpr (::std::is_trivially_default_constructible_v<value_type> &&
					  ::std::is_trivially_destructible_v<value_type>)
		{
			this->resize_for_overwrite_impl(count);
		}
#if 0
		else if constexpr(::fast_io::freestanding::is_zero_default_constructible_v<value_type>&&
			::std::is_trivially_destructible_v<value_type>)
		{
		}
#endif
		else
		{
			this->resize(count);
		}
	}
	inline constexpr void resize(size_type count, const_reference value) noexcept(::std::is_nothrow_copy_constructible_v<value_type> && ::std::is_nothrow_move_constructible_v<value_type>)
	{
		this->resize_impl(count, __builtin_addressof(value));
	}

private:
	inline constexpr iterator erase_no_destroy_common_impl(iterator first, iterator last, bool moveleft) noexcept
	{
		::fast_io::containers::details::deque_control_block<value_type> back_block;
		if (moveleft)
		{
			auto ed{this->end()};
			auto front_block{::fast_io::freestanding::uninitialized_relocate_backward(this->begin(), first, last).itercontent};
			back_block = this->controller.back_block;
			if (last == ed)
			{
				front_block = back_block;
			}
			this->controller.front_block = front_block;
			this->controller.front_end_ptr = front_block.begin_ptr + block_size;
			first = last;
		}
		else
		{
			back_block = ::fast_io::freestanding::uninitialized_relocate(last, this->end(), first).itercontent;
		}

		this->set_newed_common(back_block);
		return first;
	}

	inline constexpr iterator erase_unchecked_nodestroy_impl(iterator first, iterator last, bool moveleft) noexcept
	{
		if constexpr (::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<value_type> && 0)
		{
			if !consteval
			{
				constexpr size_type blockbytes{block_size * sizeof(value_type)};
				return ::std::bit_cast<iterator>(
					::fast_io::containers::details::deque_erase_common_trivial_impl(
						*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(__builtin_addressof(
							this->controller)),
						::std::bit_cast<::fast_io::containers::details::deque_control_block_common>(first.itercontent),
						::std::bit_cast<::fast_io::containers::details::deque_control_block_common>(last.itercontent), moveleft, blockbytes));
			}
		}
		return this->erase_no_destroy_common_impl(first, last, moveleft);
	}
	inline constexpr void erase_unchecked_nodestroy_for_insert_counts_impl(iterator first, ::std::size_t count) noexcept
	{
		if (!count)
		{
			return;
		}

		::std::size_t const distofront{
			::fast_io::containers::details::deque_iter_difference_unsigned_common(first.itercontent, this->controller.front_block)};

		auto last{first + count};
		::std::size_t const distoback{
			::fast_io::containers::details::deque_iter_difference_unsigned_common(this->controller.back_block, last.itercontent)};

		this->erase_unchecked_nodestroy_impl(first, last, distofront < distoback);
	}
	inline constexpr iterator erase_unchecked_impl(iterator first, iterator last, bool moveleft) noexcept
	{
		if (first == last)
		{
			return first;
		}
		if constexpr (!::std::is_trivially_destructible_v<value_type>)
		{
			this->destroy_elements_range(first.itercontent, last.itercontent);
		}
		return this->erase_unchecked_nodestroy_impl(first, last, moveleft);
	}
	inline constexpr iterator erase_unchecked_single_nodestroy_impl(iterator pos, bool moveleft) noexcept
	{
		if constexpr (::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<value_type>)
		{
			if !consteval
			{
				auto posp1{pos.itercontent};
				if (++posp1.curr_ptr == posp1.begin_ptr + block_size)
				{
					if (posp1.curr_ptr != this->controller.back_end_ptr)
					{
						posp1.curr_ptr = posp1.begin_ptr = *(++posp1.controller_ptr);
					}
				}
				constexpr size_type blockbytes{block_size * sizeof(value_type)};
				return ::std::bit_cast<iterator>(
					::fast_io::containers::details::deque_erase_common_trivial_impl(
						*reinterpret_cast<::fast_io::containers::details::deque_controller_common *>(__builtin_addressof(
							this->controller)),
						::std::bit_cast<::fast_io::containers::details::deque_control_block_common>(pos.itercontent),
						::std::bit_cast<::fast_io::containers::details::deque_control_block_common>(posp1), moveleft, blockbytes));
			}
		}
		auto posp1{pos};
		++posp1;
		return this->erase_no_destroy_common_impl(pos, posp1, moveleft);
	}
	inline constexpr iterator erase_unchecked_single_impl(iterator pos, bool moveleft) noexcept
	{
		if constexpr (!::std::is_trivially_destructible_v<value_type>)
		{
			::std::destroy_at(pos.itercontent.curr_ptr);
		}
		return this->erase_unchecked_single_nodestroy_impl(pos, moveleft);
	}

public:
	inline constexpr iterator erase(const_iterator first, const_iterator last) noexcept
	{
		return this->erase_unchecked_impl(iterator{first.itercontent},
										  iterator{last.itercontent},
										  static_cast<size_type>(first - this->cbegin()) <
											  static_cast<size_type>(this->cend() - last));
	}

	inline constexpr size_type erase_index(size_type firstidx, size_type lastidx) noexcept
	{
		size_type const n{this->size()};
		if (n < firstidx || n < lastidx) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		auto bg{this->begin()};
		this->erase_unchecked_impl(bg + firstidx, bg + lastidx, firstidx < (n - lastidx));
		return firstidx;
	}

	inline constexpr iterator erase(const_iterator first) noexcept
	{
		::std::size_t const n{this->size()};
		::std::size_t const distofront{
			::fast_io::containers::details::deque_iter_difference_unsigned_common(first.itercontent, this->controller.front_block)};
		return this->erase_unchecked_single_impl(iterator{first.itercontent},
												 distofront < static_cast<::std::size_t>(n - distofront));
	}

	inline constexpr size_type erase_index(size_type firstidx) noexcept
	{
		size_type const n{this->size()};
		if (n <= firstidx) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		this->erase_unchecked_single_impl(this->begin() + firstidx,
										  firstidx < static_cast<size_type>(n - firstidx));
		return firstidx;
	}

	inline constexpr ~deque()
	{
		destroy_deque_controller(this->controller);
	}

	inline constexpr void swap(deque &rhs) noexcept
	{
		::std::swap(this->controller, rhs.controller);
	}
};

template <typename T, typename allocator>
inline constexpr void swap(::fast_io::containers::deque<T, allocator> &lhs, ::fast_io::containers::deque<T, allocator> &rhs) noexcept
{
	lhs.swap(rhs);
}

template <typename T, typename allocator1, typename allocator2>
	requires ::std::equality_comparable<T>
inline constexpr bool operator==(::fast_io::containers::deque<T, allocator1> const &lhs, ::fast_io::containers::deque<T, allocator2> const &rhs) noexcept
{
	return ::std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
}

#if defined(__cpp_lib_three_way_comparison)

template <typename T, typename allocator1, typename allocator2>
	requires ::std::three_way_comparable<T>
inline constexpr auto operator<=>(::fast_io::containers::deque<T, allocator1> const &lhs, ::fast_io::containers::deque<T, allocator2> const &rhs) noexcept
{
	return ::fast_io::freestanding::lexicographical_compare_three_way(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(), ::std::compare_three_way{});
}

#endif

template <typename T, typename Alloc, typename U = T>
constexpr typename ::fast_io::containers::deque<T, Alloc>::size_type
erase(::fast_io::containers::deque<T, Alloc> &c, U const &value)
{
	auto ed{c.end()};
	auto it = ::std::remove(c.begin(), ed, value);
	auto r = ::fast_io::containers::details::deque_iter_difference_unsigned_common(ed.itercontent, it.itercontent);
	c.erase(it, ed);
	return r;
}

template <typename T, typename Alloc, typename Pred>
constexpr typename ::fast_io::containers::deque<T, Alloc>::size_type
erase_if(::fast_io::containers::deque<T, Alloc> &c, Pred pred)
{
	auto ed{c.end()};
	auto it = ::std::remove_if(c.begin(), ed, pred);
	auto r = ::fast_io::containers::details::deque_iter_difference_unsigned_common(ed.itercontent, it.itercontent);
	c.erase(it, ed);
	return r;
}

} // namespace containers

namespace freestanding
{

template <typename T, typename allocator>
struct is_trivially_copyable_or_relocatable<::fast_io::containers::deque<T, allocator>>
{
	inline static constexpr bool value = true;
};

template <typename T, typename allocator>
struct is_zero_default_constructible<::fast_io::containers::deque<T, allocator>>
{
	inline static constexpr bool value = true;
};

} // namespace freestanding

} // namespace fast_io
