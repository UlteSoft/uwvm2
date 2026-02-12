// Copyright 2025-2026 YexuanXiao
// Distributed under the MIT License.
// https://github.com/YexuanXiao/deque

#if !defined(BIZWEN_DEQUE_HPP)
#define BIZWEN_DEQUE_HPP

#if !defined(BIZWEN_MODULE)
#define BIZWEN_EXPORT

// assert
#include <cassert>
// ptrdiff_t/size_t
#include <cstddef>
// ranges::copy/copy_back_ward/rotate/move/move_backward/remove/remove_if
#include <algorithm>
// strong_ordering/lexicographical_compare/lexicographical_compare_three_way
#include <compare>
// iterator concepts/reverse_iterator/sentinel/iterator tag
#include <iterator>
// construct_at/destroy_at/uninitialized algorithms
#include <memory>
// add_pointer/remove_pointer/remove_const/is_const/is_object
#include <type_traits>
// ranges::view_interface/subrange/sized_range/from_range_t/begin/end/swap/size/empty/views::all
#include <ranges>
// span
#include <span>
// move/forward
#include <utility>
// initializer_list
#include <initializer_list>
// __cpp_lib_containers_ranges/__cpp_lib_ranges_repeat
#include <version>
// polymorphic_allocator
#include <memory_resource>

#if defined(__cpp_exceptions)
// out_of_range
#include <stdexcept>
#else
// terminate
#include <exception>
#endif

#else

#define BIZWEN_EXPORT export

#endif

// 代码规范：
// 使用等号初始化
// 内部函数可以使用auto返回值
// 函数参数和常量加const
// 非API不使用size_t之外的整数
// const写右侧
// 不分配内存/构造/移动对象一律noexcept

// STL-vNext BEGIN

namespace bizwen
{
BIZWEN_EXPORT template <typename T, typename Alloc>
class deque;

namespace deque_detail
{

template <typename U, typename Alloc>
inline constexpr void destroy_range(Alloc &a, U first, U last)
{
    static_assert(::std::is_same_v<::std::iter_value_t<U>, typename ::std::allocator_traits<Alloc>::value_type>);
    for (; first != last; ++first)
    {
        ::std::allocator_traits<Alloc>::destroy(a, ::std::to_address(first));
    }
}

template <typename U, typename Alloc>
class throw_guard
{
    U first_;
    U &current_;
    Alloc &a_;
    bool flag = true;

  public:
    constexpr throw_guard(U &result, Alloc &al) noexcept : first_(result), current_(result), a_(al)
    {
    }

    constexpr ~throw_guard()
    {
        if (flag)
        {
            destroy_range(a_, first_, current_);
        }
    }

    constexpr void release() noexcept
    {
        flag = false;
    }
};

template <typename U, typename V, typename W, typename X, typename Alloc>
inline constexpr void uninitialized_copy(Alloc &a, U first, V last, W first2, X last2)
{
    throw_guard guard{first2, a};

    for (; first != last && first2 != last2; ++first, (void)++first2)
    {
        ::std::allocator_traits<Alloc>::construct(a, ::std::to_address(first2), *first);
    }

    guard.release();
}

template <typename U, typename V, typename T, typename Alloc>
inline constexpr void uninitialized_fill(Alloc &a, U first, V last, const T &value)
{
    throw_guard guard{first, a};

    for (; first != last; ++first)
    {
        ::std::allocator_traits<Alloc>::construct(a, ::std::to_address(first), value);
    }

    guard.release();
}

template <typename U, typename V, typename W, typename X, typename Alloc>
inline constexpr void uninitialized_move(Alloc &a, U first, V last, W first2, X last2)
{
    throw_guard guard{first2, a};

    for (; first != last && first2 != last2; ++first, (void)++first2)
    {
        ::std::allocator_traits<Alloc>::construct(a, ::std::to_address(first2), ::std::move(*first));
    }

    guard.release();
}

template <typename U, typename V, typename Alloc>
inline constexpr void uninitialized_value_construct(Alloc &a, U first, V last)
{
    throw_guard guard{first, a};

    for (; first != last; ++first)
    {
        ::std::allocator_traits<Alloc>::construct(a, ::std::to_address(first));
    }

    guard.release();
}

template <typename T>
struct adl_firewall_impl_
{
    // adl_firewall的关联类型包括adl_firewall_impl_<T>但不包括T
    struct adl_firewall_
    {
        using type = T;
    };
};

template <typename T>
using add_adl_firewall_t = adl_firewall_impl_<T>::adl_firewall_;

template <typename Firewall>
using remove_adl_firewall_t = Firewall::type;

template <typename A>
concept mini_alloc = requires(A &a) {
    typename A::value_type;
    a.allocate(::std::size_t(0));
};

// 用于从参数包中获得前两个对象（只有两个）的引用的辅助函数
template <typename U, typename V>
inline constexpr auto get_iter_pair(U &first, V &last) noexcept
{
    struct iter_ref_pair
    {
        decltype(first) &src_begin;
        decltype(last) &src_end;
    };
    return iter_ref_pair{first, last};
}

template <typename T>
inline constexpr auto to_address(T const t) noexcept
{
    return ::std::to_address(t);
}

inline constexpr auto to_address(::std::nullptr_t) noexcept
{
    return nullptr;
}

#if defined(BIZWEN_DEQUE_BLOCK_ELEMENTS)
template <typename T>
inline constexpr ::std::size_t block_elements_v = BIZWEN_DEQUE_BLOCK_ELEMENTS;
#else
template <typename T>
inline constexpr ::std::size_t block_elements_v = ::std::size_t(16) > ::std::size_t(4096) / sizeof(T)
                                                      ? ::std::size_t(16)
                                                      : ::std::size_t(4096) / sizeof(T);
#endif

// 构造函数和赋值用，计算如何分配和构造
template <typename T>
inline constexpr auto calc_cap(::std::size_t const size) noexcept
{
    auto const block_elems = block_elements_v<T>;
    struct cap_t
    {
        ::std::size_t block_size;  // 需要分配多少block
        ::std::size_t full_blocks; // 分配了几个完整block
        ::std::size_t rem_elems;   // 剩下的不完整block有多少元素
    };
    return cap_t{(size + block_elems - ::std::size_t(1)) / block_elems, size / block_elems, size % block_elems};
}

// 该函数计算位置，参数front_size是起始位置和块首的距离，pos是目标位置
// 对于负数pos返回负数位置
template <typename T>
inline constexpr auto calc_pos(::std::ptrdiff_t const front_size, ::std::ptrdiff_t const pos) noexcept
{
    ::std::ptrdiff_t const block_elems = block_elements_v<T>;
    struct pos_t
    {
        ::std::ptrdiff_t block_step; // 移动块的步数
        ::std::ptrdiff_t elem_step;  // 移动元素的步数（相对于块首）
    };
    if (pos >= ::std::ptrdiff_t(0))
    {
        auto const new_pos = pos + front_size;
        return pos_t{new_pos / block_elems, new_pos % block_elems};
    }
    else
    {
        auto const new_pos = pos + front_size - block_elems + ::std::ptrdiff_t(1);
        return pos_t{new_pos / block_elems, new_pos % block_elems - ::std::ptrdiff_t(1) + block_elems};
    }
}

template <typename T>
inline constexpr auto calc_pos(::std::size_t const front_size, ::std::size_t const pos) noexcept
{
    ::std::size_t const block_elems = block_elements_v<T>;
    struct pos_t
    {
        ::std::size_t block_step; // 移动块的步数
        ::std::size_t elem_step;  // 移动元素的步数（相对于块首）
    };
    auto const new_pos = pos + front_size;
    return pos_t{new_pos / block_elems, new_pos % block_elems};
}

template <typename T, typename Block, typename DiffType>
class buckets_type;

template <typename FirewallT, typename FirewallBlock, typename DiffType>
class bucket_iterator
{
    using T = deque_detail::remove_adl_firewall_t<FirewallT>;
    using Block = deque_detail::remove_adl_firewall_t<FirewallBlock>;
    static_assert(::std::is_object_v<T> && ::std::is_object_v<Block> && ::std::is_signed_v<DiffType>);
    using RConstT = ::std::remove_const_t<T>;

    friend buckets_type<deque_detail::add_adl_firewall_t<RConstT>, FirewallBlock, DiffType>;
    friend buckets_type<deque_detail::add_adl_firewall_t<T>, FirewallBlock, DiffType>;
    friend bucket_iterator<deque_detail::add_adl_firewall_t<T const>, FirewallBlock, DiffType>;

    Block *block_elem_begin_{};
    Block *block_elem_end_{};
    Block *block_elem_curr_{};
    RConstT *elem_begin_begin_{};
    RConstT *elem_begin_end_{};
    RConstT *elem_end_begin_{};
    RConstT *elem_end_end_{};
    RConstT *elem_curr_begin_{};
    RConstT *elem_curr_end_{};

    constexpr bucket_iterator(Block *const block_elem_begin, Block *const block_elem_end, Block *const block_elem_curr,
                              RConstT *const elem_begin_begin, RConstT *const elem_begin_end,
                              RConstT *const elem_end_begin, RConstT *const elem_end_end,
                              RConstT *const elem_curr_begin, RConstT *const elem_curr_end) noexcept
        : block_elem_begin_(block_elem_begin), block_elem_end_(block_elem_end), block_elem_curr_(block_elem_curr),
          elem_begin_begin_(elem_begin_begin), elem_begin_end_(elem_begin_end), elem_end_begin_(elem_end_begin),
          elem_end_end_(elem_end_end), elem_curr_begin_(elem_curr_begin), elem_curr_end_(elem_curr_end)
    {
    }

    constexpr bucket_iterator<deque_detail::add_adl_firewall_t<RConstT>, FirewallBlock, DiffType> remove_const_()
        const noexcept
        requires(::std::is_const_v<T>)
    {
        return {block_elem_begin_, block_elem_end_, block_elem_curr_, elem_begin_begin_, elem_begin_end_,
                elem_end_begin_,   elem_end_end_,   elem_curr_begin_, elem_curr_end_};
    }

    constexpr bucket_iterator &plus_and_assign_(::std::ptrdiff_t const pos) noexcept
    {
        block_elem_curr_ += pos;
        if (block_elem_curr_ + ::std::size_t(1) == block_elem_end_)
        {
            elem_curr_begin_ = elem_end_begin_;
            elem_curr_end_ = elem_end_end_;
        }
        else if (block_elem_curr_ == block_elem_begin_)
        {
            elem_curr_begin_ = elem_begin_begin_;
            elem_curr_end_ = elem_begin_end_;
        }
        else
        {
            elem_curr_begin_ = *block_elem_begin_;
            elem_curr_end_ = elem_begin_begin_ + block_elements_v<T>;
        }
        assert(block_elem_curr_ < block_elem_end_ && block_elem_curr_ >= block_elem_begin_);
        return *this;
    }

  public:
    using difference_type = DiffType;
    using value_type = ::std::span<T>;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = ::std::random_access_iterator_tag;

    constexpr bucket_iterator() noexcept = default;

    constexpr bucket_iterator(bucket_iterator const &other) noexcept = default;

    constexpr bucket_iterator &operator=(bucket_iterator const &other) noexcept = default;

    constexpr ~bucket_iterator() = default;

    constexpr bucket_iterator &operator++() noexcept
    {
        ++block_elem_curr_;
        if (block_elem_curr_ + ::std::size_t(1) == block_elem_end_)
        {
            elem_curr_begin_ = elem_end_begin_;
            elem_curr_end_ = elem_end_end_;
        }
        else
        {
            elem_curr_begin_ = *block_elem_begin_;
            elem_curr_end_ = elem_begin_begin_ + block_elements_v<T>;
        }
        assert(block_elem_curr_ < block_elem_end_);
        return *this;
    }

    constexpr bucket_iterator operator++(int) noexcept
    {
#if defined(__cpp_auto_cast)
        return ++auto{*this};
#else
        auto temp = *this;
        ++temp;
        return temp;
#endif
    }

    constexpr bucket_iterator &operator--() noexcept
    {
        --block_elem_curr_;
        if (block_elem_curr_ == block_elem_begin_)
        {
            elem_curr_begin_ = elem_begin_begin_;
            elem_curr_end_ = elem_begin_end_;
        }
        else
        {
            elem_begin_begin_ = *(block_elem_begin_ - ::std::size_t(1));
            elem_begin_end_ = elem_begin_begin_ + block_elements_v<T>;
        }
        assert(block_elem_curr_ >= block_elem_begin_);
        return *this;
    }

    constexpr bucket_iterator operator--(int) noexcept
    {
#if defined(__cpp_auto_cast)
        return --auto{*this};
#else
        auto temp = *this;
        --temp;
        return temp;
#endif
    }

    constexpr bool operator==(bucket_iterator const &other) const noexcept
    {
        return block_elem_curr_ == other.block_elem_curr_;
    }

    constexpr ::std::strong_ordering operator<=>(bucket_iterator const &other) const noexcept
    {
        return block_elem_curr_ <=> other.block_elem_curr_;
    }

    constexpr difference_type operator-(bucket_iterator const &other) const noexcept
    {
        return block_elem_curr_ - other.block_elem_curr_;
    }

    constexpr ::std::span<T> operator[](difference_type const pos)
    {
#if defined(__cpp_auto_cast)
        return *(auto{*this} += pos);
#else
        auto temp = *this;
        temp += pos;
        return *temp;
#endif
    }

    constexpr ::std::span<T> operator[](difference_type const pos) const noexcept
    {
#if defined(__cpp_auto_cast)
        return *(auto{*this} += pos);
#else
        auto temp = *this;
        temp += pos;
        return *temp;
#endif
    }

    constexpr value_type operator*() noexcept
    {
        return {elem_curr_begin_, elem_curr_end_};
    }

    constexpr value_type operator*() const noexcept
    {
        return {elem_curr_begin_, elem_curr_end_};
    }

    constexpr bucket_iterator &operator+=(difference_type const pos) noexcept
    {
        return plus_and_assign_(pos);
    }

    constexpr bucket_iterator &operator-=(difference_type const pos) noexcept
    {
        return plus_and_assign_(-pos);
    }

    friend constexpr bucket_iterator operator+(bucket_iterator const &it, difference_type const pos) noexcept
    {
#if defined(__cpp_auto_cast)
        return auto{it}.plus_and_assign_(pos);
#else
        auto temp = it;
        temp.plus_and_assign_(pos);
        return temp;
#endif
    }

    friend constexpr bucket_iterator operator+(difference_type const pos, bucket_iterator const &it) noexcept
    {
        return it + pos;
    }

    friend constexpr bucket_iterator operator-(difference_type const pos, bucket_iterator const &it) noexcept
    {
        return it + (-pos);
    }

    friend constexpr bucket_iterator operator-(bucket_iterator const &it, difference_type pos) noexcept
    {
        return it + (-pos);
    }

    constexpr operator bucket_iterator<deque_detail::add_adl_firewall_t<T const>,
                                       deque_detail::add_adl_firewall_t<Block>, DiffType>() const noexcept
        requires(!::std::is_const_v<T>)
    {
        return {block_elem_begin_, block_elem_end_, block_elem_curr_, elem_begin_begin_, elem_begin_end_,
                elem_end_begin_,   elem_end_end_,   elem_curr_begin_, elem_curr_end_};
    }
};

#if !defined(NDEBUG)
static_assert(::std::random_access_iterator<bucket_iterator<
                  deque_detail::add_adl_firewall_t<int>, deque_detail::add_adl_firewall_t<int *>, ::std::ptrdiff_t>>);
static_assert(
    ::std::random_access_iterator<bucket_iterator<deque_detail::add_adl_firewall_t<const int>,
                                                  deque_detail::add_adl_firewall_t<int *>, ::std::ptrdiff_t>>);
#endif

template <typename T, typename Block, typename DiffType>
class deque_iterator;

template <typename FirewallT, typename FirewallBlock, typename DiffType>
class buckets_type : public ::std::ranges::view_interface<buckets_type<FirewallT, FirewallBlock, DiffType>>
{
    using T = deque_detail::remove_adl_firewall_t<FirewallT>;
    using Block = deque_detail::remove_adl_firewall_t<FirewallBlock>;
    static_assert(::std::is_object_v<T> && ::std::is_object_v<Block> && ::std::is_signed_v<DiffType>);

    using RConstT = ::std::remove_const_t<T>;

    template <typename U, typename Alloc>
    friend class bizwen::deque;
    friend buckets_type<deque_detail::add_adl_firewall_t<RConstT>, FirewallBlock, DiffType>;
    friend deque_iterator<deque_detail::add_adl_firewall_t<T const>, FirewallBlock, DiffType>;
    friend deque_iterator<deque_detail::add_adl_firewall_t<RConstT>, FirewallBlock, DiffType>;

    Block *block_elem_begin_{};
    Block *block_elem_end_{};
    RConstT *elem_begin_begin_{};
    RConstT *elem_begin_end_{};
    RConstT *elem_end_begin_{};
    RConstT *elem_end_end_{};

    template <typename U, typename V>
    constexpr buckets_type(U const block_elem_begin, U const block_elem_end, V const elem_begin_begin,
                           V const elem_begin_end, V const elem_end_begin, V const elem_end_end) noexcept
        : block_elem_begin_(::std::to_address(block_elem_begin)), block_elem_end_(::std::to_address(block_elem_end)),
          elem_begin_begin_(::std::to_address(elem_begin_begin)), elem_begin_end_(::std::to_address(elem_begin_end)),
          elem_end_begin_(::std::to_address(elem_end_begin)), elem_end_end_(::std::to_address(elem_end_end))
    {
    }

    constexpr ::std::span<T> at_impl(::std::size_t const pos) const noexcept
    {
        assert(block_elem_begin_ + pos <= block_elem_end_);
        if (pos == ::std::size_t(0))
        {
            return {elem_begin_begin_, elem_begin_end_};
        }
        else if (block_elem_begin_ + pos + ::std::size_t(1) == block_elem_end_)
        {
            return {elem_end_begin_, elem_end_end_};
        }
        else
        {
            auto const begin = *(block_elem_begin_ + pos);
            return {begin, begin + block_elements_v<T>};
        }
    }

  public:
    using value_type = ::std::span<T>;
    using pointer = value_type *;
    using reference = value_type &;
    using const_pointer = value_type const *;
    using const_reference = value_type const &;
    using size_type = ::std::make_unsigned_t<DiffType>;
    using difference_type = DiffType;
    using iterator =
        bucket_iterator<deque_detail::add_adl_firewall_t<T>, deque_detail::add_adl_firewall_t<Block>, DiffType>;
    using const_iterator =
        bucket_iterator<deque_detail::add_adl_firewall_t<T const>, deque_detail::add_adl_firewall_t<Block>, DiffType>;
    using reverse_iterator = ::std::reverse_iterator<iterator>;
    using const_reverse_iterator = ::std::reverse_iterator<const_iterator>;

    constexpr buckets_type() = default;

    constexpr ~buckets_type() = default;

    constexpr buckets_type(buckets_type const &) = default;

    constexpr buckets_type &operator=(buckets_type const &) = default;

    constexpr size_type size() const noexcept
    {
        return static_cast<size_type>(block_elem_end_ - block_elem_begin_);
    }

    // empty and operator bool provided by view_interface

    // Since bucket_iterator is not a continuous iterator,
    // view_interface does not provide the member function data
    // constexpr void data() const noexcept = delete;

    constexpr ::std::span<T> front() const noexcept
    {
        return {elem_begin_begin_, elem_begin_end_};
    }

    constexpr ::std::span<T const> front() noexcept
        requires(!::std::is_const_v<T>)
    {
        return {elem_begin_begin_, elem_begin_end_};
    }

    constexpr ::std::span<T> back() const noexcept
    {
        return {elem_end_begin_, elem_end_end_};
    }

    constexpr ::std::span<T const> back() noexcept
        requires(!::std::is_const_v<T>)
    {
        return {elem_end_begin_, elem_end_end_};
    }

    constexpr ::std::span<T> at(size_type const pos) noexcept
    {
        return at_impl(pos);
    }

    constexpr ::std::span<const T> at(size_type const pos) const noexcept
        requires(!::std::is_const_v<T>)
    {
        auto const s = at_impl(pos);
        return {s.data(), s.size()};
    }

    constexpr const_iterator begin() const noexcept
    {
        return {block_elem_begin_, block_elem_end_, block_elem_begin_, elem_begin_begin_, elem_begin_end_,
                elem_end_begin_,   elem_end_end_,   elem_begin_begin_, elem_begin_end_};
    }

    constexpr const_iterator end() const noexcept
    {
        if (block_elem_begin_ == block_elem_end_)
        {
            return {block_elem_begin_, block_elem_end_, block_elem_end_, elem_begin_begin_, elem_begin_end_,
                    elem_end_begin_,   elem_end_end_,   elem_end_begin_, elem_end_end_};
        }
        else
        {
            return {block_elem_begin_, block_elem_end_, block_elem_end_ - ::std::size_t(1),
                    elem_begin_begin_, elem_begin_end_, elem_end_begin_,
                    elem_end_end_,     elem_end_begin_, elem_end_end_};
        }
    }

    constexpr iterator begin() noexcept
    {
        return static_cast<buckets_type const &>(*this).begin().remove_const_();
    }

    constexpr iterator end() noexcept
    {
        return static_cast<buckets_type const &>(*this).end().remove_const_();
    }

    constexpr const_iterator cbegin() const noexcept
    {
        return begin();
    }

    constexpr const_iterator cend() const noexcept
    {
        return end();
    }

    constexpr auto rbegin() noexcept
    {
        return reverse_iterator{end()};
    }

    constexpr auto rend() noexcept
    {
        return reverse_iterator{begin()};
    }

    constexpr auto rbegin() const noexcept
    {
        return const_reverse_iterator{end()};
    }

    constexpr auto rend() const noexcept
    {
        return const_reverse_iterator{begin()};
    }

    constexpr auto crbegin() const noexcept
    {
        return const_reverse_iterator{end()};
    }

    constexpr auto crend() const noexcept
    {
        return const_reverse_iterator{begin()};
    }

    constexpr operator buckets_type<deque_detail::add_adl_firewall_t<T const>, deque_detail::add_adl_firewall_t<Block>,
                                    DiffType>() const noexcept
        requires(!::std::is_const_v<T>)
    {
        return {block_elem_begin_, block_elem_end_, elem_begin_begin_, elem_begin_end_, elem_end_begin_, elem_end_end_};
    }
};

template <typename FirewallT, typename FirewallBlock, typename DiffType>
class deque_iterator
{
    using T = deque_detail::remove_adl_firewall_t<FirewallT>;
    using Block = deque_detail::remove_adl_firewall_t<FirewallBlock>;
    static_assert(::std::is_object_v<T> && ::std::is_object_v<Block> && ::std::is_signed_v<DiffType>);

    using RConstT = ::std::remove_const_t<T>;

    template <typename U, typename Alloc>
    friend class bizwen::deque;
    friend deque_iterator<deque_detail::add_adl_firewall_t<T const>, FirewallBlock, DiffType>;
    friend deque_iterator<deque_detail::add_adl_firewall_t<RConstT>, FirewallBlock, DiffType>;

    Block *block_elem_curr_{};
    Block *block_elem_end_{};
    RConstT *elem_begin_{};
    RConstT *elem_curr_{};
#if !defined(NDEBUG)
    buckets_type<deque_detail::add_adl_firewall_t<T const>, deque_detail::add_adl_firewall_t<Block>, DiffType>
        buckets_{};

    constexpr bool verify() const noexcept
    {
        assert(block_elem_curr_ >= buckets_.block_elem_begin_);
        assert(block_elem_end_ == buckets_.block_elem_end_);
        if (block_elem_curr_ != nullptr)
        {
            assert(block_elem_curr_ < block_elem_end_);
            assert(elem_begin_ == ::std::to_address(*block_elem_curr_));
        }
        assert(elem_curr_ >= elem_begin_);
        assert(elem_curr_ <= elem_begin_ + block_elements_v<T>);
        if (block_elem_curr_ == buckets_.block_elem_end_ - ::std::size_t(1))
        {
            assert(elem_curr_ <= buckets_.elem_end_end_);
        }
        if (block_elem_curr_ == buckets_.block_elem_begin_)
        {
            assert(elem_curr_ >= buckets_.elem_begin_begin_);
        }

        return true;
    }

    constexpr deque_iterator(
        Block *block_curr, Block *block_end, RConstT *const begin, RConstT *const pos,
        buckets_type<deque_detail::add_adl_firewall_t<T const>, deque_detail::add_adl_firewall_t<Block>, DiffType>
            buckets) noexcept
        : block_elem_curr_(block_curr), block_elem_end_(block_end), elem_begin_(deque_detail::to_address(begin)),
          elem_curr_(deque_detail::to_address(pos)), buckets_(buckets)
    {
    }
#else
    constexpr deque_iterator(Block *block_curr, Block *block_end, RConstT *const begin, RConstT *const pos) noexcept
        : block_elem_curr_(block_curr), block_elem_end_(block_end), elem_begin_(deque_detail::to_address(begin)),
          elem_curr_(deque_detail::to_address(pos))
    {
    }
#endif

    constexpr deque_iterator<deque_detail::add_adl_firewall_t<RConstT>, deque_detail::add_adl_firewall_t<Block>,
                             DiffType> remove_const_() const noexcept
        requires(::std::is_const_v<T>)
    {
#if !defined(NDEBUG)
        return {block_elem_curr_, block_elem_end_, elem_begin_, elem_curr_, buckets_};
#else
        return {block_elem_curr_, block_elem_end_, elem_begin_, elem_curr_};
#endif
    }

    constexpr T &at_impl_(::std::ptrdiff_t const pos) const noexcept
    {
        assert(verify());
        auto const res = deque_detail::calc_pos<T>(elem_curr_ - elem_begin_, pos);
        auto const target_block = block_elem_curr_ + res.block_step;
        assert(target_block < block_elem_end_);
        return *((*target_block) + res.elem_step);
    }

    constexpr deque_iterator &plus_and_assign_(::std::ptrdiff_t const pos) noexcept
    {
        assert(verify());
        if (pos != ::std::ptrdiff_t(0))
        {
            auto const res = deque_detail::calc_pos<T>(elem_curr_ - elem_begin_, pos);
            auto const target_block = block_elem_curr_ + res.block_step;
            if (target_block < block_elem_end_)
            {
                block_elem_curr_ = target_block;
                elem_begin_ = ::std::to_address(*target_block);
                elem_curr_ = elem_begin_ + res.elem_step;
            }
            else
            {
                assert(target_block == block_elem_end_);
                assert(res.elem_step == ::std::size_t(0));
                block_elem_curr_ = target_block - ::std::size_t(1);
                elem_begin_ = ::std::to_address(*(target_block - ::std::size_t(1)));
                elem_curr_ = elem_begin_ + deque_detail::block_elements_v<T>;
            }
        }
        assert(verify());
        return *this;
    }

  public:
    using difference_type = DiffType;
    using value_type = RConstT;
    using pointer = T *;
    using reference = T &;
    using iterator_category = ::std::random_access_iterator_tag;

    constexpr deque_iterator() noexcept = default;

    constexpr deque_iterator(deque_iterator const &other) noexcept = default;

    constexpr deque_iterator &operator=(deque_iterator const &other) noexcept = default;

    constexpr ~deque_iterator() = default;

    constexpr bool operator==(deque_iterator const &other) const noexcept
    {
        return elem_curr_ == other.elem_curr_;
    }

    constexpr ::std::strong_ordering operator<=>(deque_iterator const &other) const noexcept
    {
        assert(block_elem_end_ == other.block_elem_end_);
        if (block_elem_curr_ < other.block_elem_curr_)
            return ::std::strong_ordering::less;
        if (block_elem_curr_ > other.block_elem_curr_)
            return ::std::strong_ordering::greater;
        if (elem_curr_ < other.elem_curr_)
            return ::std::strong_ordering::less;
        if (elem_curr_ > other.elem_curr_)
            return ::std::strong_ordering::greater;
        return ::std::strong_ordering::equal;
    }

    constexpr T *operator->() noexcept
    {
        assert(elem_curr_ != elem_begin_ + deque_detail::block_elements_v<T>);
        return elem_curr_;
    }

    constexpr T *operator->() const noexcept
    {
        assert(elem_curr_ != elem_begin_ + deque_detail::block_elements_v<T>);
        return elem_curr_;
    }

    constexpr T &operator*() noexcept
    {
        return *this->operator->();
    }

    constexpr T &operator*() const noexcept
    {
        return *this->operator->();
    }

    constexpr deque_iterator &operator++() noexcept
    {
        assert(verify());
        // 空deque的迭代器不能自增，不需要考虑
        assert(elem_curr_ != elem_begin_ + deque_detail::block_elements_v<T>);
        ++elem_curr_;
        if (elem_curr_ == elem_begin_ + deque_detail::block_elements_v<T>)
        {
            if (block_elem_curr_ + ::std::size_t(1) != block_elem_end_)
            {
                ++block_elem_curr_;
                elem_begin_ = ::std::to_address(*block_elem_curr_);
                elem_curr_ = elem_begin_;
            }
        }
        assert(verify());
        return *this;
    }

    constexpr deque_iterator operator++(int) noexcept
    {
#if defined(__cpp_auto_cast)
        return ++auto{*this};
#else
        auto temp = *this;
        ++temp;
        return temp;
#endif
    }

    constexpr deque_iterator &operator--() noexcept
    {
        assert(verify());
        if (elem_curr_ == elem_begin_)
        {
            --block_elem_curr_;
            elem_begin_ = ::std::to_address(*block_elem_curr_);
            elem_curr_ = elem_begin_ + deque_detail::block_elements_v<T>;
        }
        --elem_curr_;
        assert(verify());
        return *this;
    }

    constexpr deque_iterator operator--(int) noexcept
    {
#if defined(__cpp_auto_cast)
        return --auto{*this};
#else
        auto temp = *this;
        --temp;
        return temp;
#endif
    }

    constexpr T &operator[](difference_type const pos) noexcept
    {
        return at_impl_(pos);
    }

    constexpr T &operator[](difference_type const pos) const noexcept
    {
        return at_impl_(pos);
    }

    friend constexpr difference_type operator-(deque_iterator const &lhs, deque_iterator const &rhs) noexcept
    {
        assert(lhs.block_elem_end_ == rhs.block_elem_end_);
        auto const block_size = lhs.block_elem_curr_ - rhs.block_elem_curr_;
        return static_cast<difference_type>(block_size * static_cast<difference_type>(block_elements_v<T>) +
                                            lhs.elem_curr_ - lhs.elem_begin_ - (rhs.elem_curr_ - rhs.elem_begin_));
    }

    constexpr deque_iterator &operator+=(difference_type const pos) noexcept
    {
        return plus_and_assign_(pos);
    }

    friend constexpr deque_iterator operator+(deque_iterator const &it, difference_type const pos) noexcept
    {
#if defined(__cpp_auto_cast)
        return auto{it}.plus_and_assign_(pos);
#else
        auto temp = it;
        temp.plus_and_assign_(pos);
        return temp;
#endif
    }

    friend constexpr deque_iterator operator+(difference_type const pos, deque_iterator const &it) noexcept
    {
        return it + pos;
    }

    constexpr deque_iterator &operator-=(difference_type const pos) noexcept
    {
        return plus_and_assign_(-pos);
    }

    friend constexpr deque_iterator operator-(deque_iterator const &it, difference_type const pos) noexcept
    {
        return it + (-pos);
    }

    friend constexpr deque_iterator operator-(difference_type const pos, deque_iterator const &it) noexcept
    {
        return it + (-pos);
    }

    constexpr operator deque_iterator<deque_detail::add_adl_firewall_t<T const>,
                                      deque_detail::add_adl_firewall_t<Block>, DiffType>() const noexcept
        requires(!::std::is_const_v<T>)
    {
#if !defined(NDEBUG)
        return {block_elem_curr_, block_elem_end_, elem_begin_, elem_curr_, buckets_};
#else
        return {block_elem_curr_, block_elem_end_, elem_begin_, elem_curr_};
#endif
    }
};

#if !defined(__cpp_lib_ranges_repeat) || 1
template <typename T>
class repeat_iterator
{
    ::std::ptrdiff_t pos_{};
    T const *value_ptr_{};

  public:
    using iterator_category = ::std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = ::std::ptrdiff_t;
    using pointer = T const *;
    using reference = T const &;

    constexpr repeat_iterator() = default;

    constexpr repeat_iterator(difference_type pos, T const &value) noexcept
        : pos_(pos), value_ptr_(::std::addressof(value))
    {
    }

    constexpr reference operator*() const noexcept
    {
        assert(value_ptr_ != nullptr);
        return *value_ptr_;
    }

    constexpr pointer operator->() const noexcept
    {
        assert(value_ptr_ != nullptr);
        return value_ptr_;
    }

    constexpr reference operator[](difference_type) const noexcept
    {
        return *value_ptr_;
    }

    constexpr repeat_iterator &operator++() noexcept
    {
        ++pos_;
        return *this;
    }

    constexpr repeat_iterator operator++(int) noexcept
    {
        auto temp = *this;
        ++*this;
        return temp;
    }

    constexpr repeat_iterator &operator--() noexcept
    {
        --pos_;
        return *this;
    }

    constexpr repeat_iterator operator--(int) noexcept
    {
        auto temp = *this;
        --*this;
        return temp;
    }

    constexpr repeat_iterator &operator+=(difference_type n) noexcept
    {
        pos_ += n;
        return *this;
    }

    constexpr repeat_iterator &operator-=(difference_type n) noexcept
    {
        pos_ -= n;
        return *this;
    }

    constexpr repeat_iterator operator+(difference_type n) const noexcept
    {
        auto temp = *this;
        temp += n;
        return temp;
    }

    constexpr repeat_iterator operator-(difference_type n) const noexcept
    {
        auto temp = *this;
        temp -= n;
        return temp;
    }

    friend constexpr difference_type operator-(repeat_iterator const &lhs, repeat_iterator const &rhs) noexcept
    {
        assert(lhs.value_ptr_ == rhs.value_ptr_);
        return lhs.pos_ - rhs.pos_;
    }

    friend constexpr auto operator<=>(repeat_iterator const &lhs, repeat_iterator const &rhs) noexcept
    {
        assert(lhs.value_ptr_ == rhs.value_ptr_);
        return lhs.pos_ <=> rhs.pos_;
    }

    friend constexpr bool operator==(repeat_iterator const &lhs, repeat_iterator const &rhs) noexcept
    {
        assert(lhs.value_ptr_ == rhs.value_ptr_);
        return lhs.pos_ == rhs.pos_;
    }

    friend constexpr repeat_iterator operator+(difference_type n, repeat_iterator const &it) noexcept
    {
        return it + n;
    }
};
#if !defined(NDEBUG)
static_assert(::std::random_access_iterator<repeat_iterator<int>>);
#endif
#endif
} // namespace deque_detail

template <typename T, typename Alloc = ::std::allocator<T>>
class deque
{
#if !defined(NDEBUG)
    static_assert(::std::is_object_v<T>);
    static_assert(!::std::is_const_v<T>);
    static_assert(::std::is_same_v<T, typename Alloc::value_type>);
#endif
    using atraits_t_ = ::std::allocator_traits<Alloc>;

    static constexpr bool is_default_operation_ = requires(Alloc &a) { a.construct(static_cast<T *>(nullptr)); };
    static constexpr bool is_aleq_ = atraits_t_::is_always_equal::value;
    static constexpr bool is_pocca_ = atraits_t_::propagate_on_container_copy_assignment::value;
    static constexpr bool is_pocma_ = atraits_t_::propagate_on_container_move_assignment::value;
    static constexpr bool is_pocs_ = atraits_t_::propagate_on_container_swap::value;

    // 给natvis使用，注意不要在其它函数中使用它，以支持使用不完整类型实例化。
    static inline constexpr ::std::size_t block_elements = deque_detail::block_elements_v<T>;

#if __has_cpp_attribute(msvc::no_unique_address)
    [[msvc::no_unique_address]] Alloc allocator_{};
#else
    [[no_unique_address]] Alloc allocator_{};
#endif

    using Block = typename atraits_t_::pointer;
    using BlockFP = typename atraits_t_::template rebind_traits<Block>::pointer;

    // 块数组的起始地址
    BlockFP block_ctrl_begin_fancy_{};
    // 块数组的结束地址
    Block *block_ctrl_end_{};
    // 已分配块的起始地址
    Block *block_alloc_begin_{};
    // 已分配块结束地址
    Block *block_alloc_end_{};
    // 已用块的首地址
    Block *block_elem_begin_{};
    // 已用块的结束地址
    Block *block_elem_end_{};
    // 首个有效块的起始分配地址
    T *elem_begin_first_{};
    // 首个有效块的首元素地址
    T *elem_begin_begin_{};
    // 首个有效块的结束分配以及尾后元素地址
    T *elem_begin_end_{};
    // 有效末尾块的起始分配以及起始元素地址
    T *elem_end_begin_{};
    // 有效末尾块的尾后元素地址
    T *elem_end_end_{};
    // 有效末尾块的结束分配地址
    T *elem_end_last_{};
    /*
  ctrl_begin→ □
             □
  alloc_begin→■ → □□□□□□□□□□□□□□□□□□□□□□□□□□
             ■ → □□□□□□□□□□□□□□□□□□□□□□□□□□
  elem_begin →■ → □□□□□□□□□■■■■■■■■■■■■■■■■■■
                 ↑          ↑                   ↑
            first      begin                end
             ■ → ■■■■■■■■■■■■■■■■■■■■■■■■■■■■
             ■ → ■■■■■■■■■■■■■■■■■■■■■■■■■■■■
  elem_end   →■ → ■■■■■■■■■■■■■□□□□□□□□□□□□□□
                 ↑              ↑               ↑
             begin           end            last
             ■ → □□□□□□□□□□□□□□□□□□□□□□□□□□
             ■ → □□□□□□□□□□□□□□□□□□□□□□□□□□
  alloc_end  →□
             □
  ctrl_end   →
    */

    constexpr Block *block_ctrl_begin_() const noexcept
    {
        return deque_detail::to_address(block_ctrl_begin_fancy_);
    }

    constexpr void dealloc_block_range_(Block *begin, Block *end) noexcept
    {
        for (; begin != end; ++begin)
        {
            atraits_t_::deallocate(allocator_, *begin, deque_detail::block_elements_v<T>);
        }
    }

    constexpr Block alloc_block_()
    {
        return atraits_t_::allocate(allocator_, deque_detail::block_elements_v<T>);
    }

    constexpr BlockFP alloc_ctrl_(::std::size_t const size)
    {
        return typename atraits_t_::template rebind_alloc<Block>(allocator_)
            .allocate(typename atraits_t_::template rebind_traits<Block>::size_type(size));
    }

    constexpr void dealloc_ctrl_() noexcept
    {
        if (block_ctrl_end_ != block_ctrl_begin_())
        {
            typename atraits_t_::template rebind_alloc<Block>(allocator_)
                .deallocate(block_ctrl_begin_fancy_, typename atraits_t_::template rebind_traits<Block>::size_type(
                                                         block_ctrl_end_ - block_ctrl_begin_()));
        }
    }

    // 空deque安全，但执行后必须手动维护状态合法
    constexpr void destroy_elems_() noexcept
    {
        if constexpr (!(::std::is_trivially_destructible_v<T> && is_default_operation_))
        {
            // 4种情况，0，1，2，3+个块有元素
            auto const block_size = block_elem_size_();
            if (block_size)
            {
                deque_detail::destroy_range(allocator_, elem_begin_begin_, elem_begin_end_);
            }
            // 清理中间的块
            if (block_size > ::std::size_t(2))
            {
                for (auto const block_begin :
                     ::std::ranges::subrange{block_elem_begin_ + ::std::size_t(1), block_elem_end_ - ::std::size_t(1)})
                {
                    deque_detail::destroy_range(allocator_, ::std::to_address(block_begin),
                                                ::std::to_address(block_begin) + deque_detail::block_elements_v<T>);
                }
            }
            if (block_size > ::std::size_t(1))
            {
                deque_detail::destroy_range(allocator_, elem_end_begin_, elem_end_end_);
            }
        }
    }

    // 完全等于析构函数
    constexpr void destroy_() noexcept
    {
        destroy_elems_();
        // 清理块数组
        dealloc_block_range_(block_alloc_begin_, block_alloc_end_);
        dealloc_ctrl_();
    }

    constexpr void clear_full_() noexcept
    {
        destroy_();
        block_ctrl_begin_fancy_ = nullptr;
        block_ctrl_end_ = nullptr;
        block_alloc_begin_ = nullptr;
        block_alloc_end_ = nullptr;
        block_elem_begin_ = nullptr;
        block_elem_end_ = nullptr;
        elem_begin_(nullptr, nullptr, nullptr);
        elem_end_(nullptr, nullptr, nullptr);
    }

    template <typename U, typename V, typename W>
    constexpr void elem_begin_(U const begin, V const end, W const first) noexcept
    {
        elem_begin_begin_ = deque_detail::to_address(begin);
        elem_begin_end_ = deque_detail::to_address(end);
        elem_begin_first_ = deque_detail::to_address(first);
    }

    template <typename U, typename V, typename W>
    constexpr void elem_end_(U const begin, V const end, W const last) noexcept
    {
        elem_end_begin_ = deque_detail::to_address(begin);
        elem_end_end_ = deque_detail::to_address(end);
        elem_end_last_ = deque_detail::to_address(last);
    }

    constexpr ::std::size_t block_elem_size_() const noexcept
    {
        return static_cast<::std::size_t>(block_elem_end_ - block_elem_begin_);
    }

    constexpr ::std::size_t block_ctrl_size_() const noexcept
    {
        return static_cast<::std::size_t>(block_ctrl_end_ - block_ctrl_begin_());
    }

    constexpr ::std::size_t block_alloc_size_() const noexcept
    {
        return static_cast<::std::size_t>(block_alloc_end_ - block_alloc_begin_);
    }

    constexpr void swap_without_ator_(deque &other) noexcept
    {
        using ::std::swap;
        swap(block_ctrl_begin_fancy_, other.block_ctrl_begin_fancy_);
        swap(block_ctrl_end_, other.block_ctrl_end_);
        swap(block_alloc_begin_, other.block_alloc_begin_);
        swap(block_alloc_end_, other.block_alloc_end_);
        swap(block_elem_begin_, other.block_elem_begin_);
        swap(block_elem_end_, other.block_elem_end_);
        swap(elem_begin_first_, other.elem_begin_first_);
        swap(elem_begin_begin_, other.elem_begin_begin_);
        swap(elem_begin_end_, other.elem_begin_end_);
        swap(elem_end_begin_, other.elem_end_begin_);
        swap(elem_end_end_, other.elem_end_end_);
        swap(elem_end_last_, other.elem_end_last_);
    }

  public:
    using value_type = T;
    using pointer = atraits_t_::pointer;
    using reference = value_type &;
    using const_pointer = atraits_t_::const_pointer;
    using const_reference = value_type const &;
    using size_type = atraits_t_::size_type;
    using difference_type = atraits_t_::difference_type;
    using iterator = deque_detail::deque_iterator<deque_detail::add_adl_firewall_t<T>,
                                                  deque_detail::add_adl_firewall_t<Block>, difference_type>;
    using reverse_iterator = ::std::reverse_iterator<iterator>;
    using const_iterator = deque_detail::deque_iterator<deque_detail::add_adl_firewall_t<T const>,
                                                        deque_detail::add_adl_firewall_t<Block>, difference_type>;
    using const_reverse_iterator = ::std::reverse_iterator<const_iterator>;
    using buckets_type = deque_detail::buckets_type<deque_detail::add_adl_firewall_t<T>,
                                                    deque_detail::add_adl_firewall_t<Block>, difference_type>;
    using const_buckets_type = deque_detail::buckets_type<deque_detail::add_adl_firewall_t<T const>,
                                                          deque_detail::add_adl_firewall_t<Block>, difference_type>;
    using allocator_type = Alloc;

    constexpr Alloc get_allocator() const noexcept
    {
        return allocator_;
    }

    constexpr buckets_type buckets() noexcept
    {
        return {block_elem_begin_, block_elem_end_, elem_begin_begin_, elem_begin_end_, elem_end_begin_, elem_end_end_};
    }

    constexpr const_buckets_type buckets() const noexcept
    {
        return {block_elem_begin_, block_elem_end_, elem_begin_begin_, elem_begin_end_, elem_end_begin_, elem_end_end_};
    }

    constexpr ~deque()
    {
        destroy_();
    }

    constexpr bool empty() const noexcept
    {
        return elem_begin_begin_ == nullptr;
    }

    constexpr void clear() noexcept
    {
        destroy_elems_();
        block_elem_begin_ = block_alloc_begin_;
        block_elem_end_ = block_alloc_begin_;
        elem_begin_(nullptr, nullptr, nullptr);
        elem_end_(nullptr, nullptr, nullptr);
    }

    constexpr void swap(deque &other) noexcept
    {
        if constexpr (!is_pocs_)
        {
            // P0178?
            if constexpr (!is_aleq_)
            {
                assert(allocator_ == other.allocator_);
            }
        }
        else
        {
            using ::std::swap;
            swap(allocator_, other.allocator_);
        }
        swap_without_ator_(other);
    }

    friend constexpr void swap(deque &lhs, deque &rhs) noexcept
    {
        lhs.swap(rhs);
    }

    // 空deque安全
    constexpr size_type size() const noexcept
    {
        auto const block_size = block_elem_size_();
        auto result = ::std::size_t(0);
        if (block_size)
        {
            result += elem_begin_end_ - elem_begin_begin_;
        }
        if (block_size > ::std::size_t(2))
        {
            result += (block_size - ::std::size_t(2)) * deque_detail::block_elements_v<T>;
        }
        if (block_size > ::std::size_t(1))
        {
            result += elem_end_end_ - elem_end_begin_;
        }
        return static_cast<size_type>(result);
    }

    constexpr size_type max_size() const noexcept
    {
        return static_cast<size_type>(
            (::std::min)(atraits_t_::max_size(allocator_), ::std::size_t(-1) / ::std::size_t(2) / sizeof(T)));
    }

#if !defined(NDEBUG)
    static_assert(::std::random_access_iterator<iterator>);
    static_assert(::std::sentinel_for<iterator, iterator>);
#endif

    constexpr const_iterator begin() const noexcept
    {
        if (block_elem_size_() == ::std::size_t(0))
        {
            return {};
        }
#if !defined(NDEBUG)
        return {block_elem_begin_, block_elem_end_, ::std::to_address(*block_elem_begin_), elem_begin_begin_,
                buckets()};
#else
        return {block_elem_begin_, block_elem_end_, ::std::to_address(*block_elem_begin_), elem_begin_begin_};
#endif
    }

    constexpr const_iterator end() const noexcept
    {
        if (block_elem_size_() == ::std::size_t(0))
        {
            return {};
        }
#if !defined(NDEBUG)
        return {block_elem_end_ - ::std::size_t(1), block_elem_end_,
                ::std::to_address(*(block_elem_end_ - ::std::size_t(1))), elem_end_end_, buckets()};
#else
        return {block_elem_end_ - ::std::size_t(1), block_elem_end_,
                ::std::to_address(*(block_elem_end_ - ::std::size_t(1))), elem_end_end_};
#endif
    }

    constexpr iterator begin() noexcept
    {
        return static_cast<const deque &>(*this).begin().remove_const_();
    }

    constexpr iterator end() noexcept
    {
        return static_cast<const deque &>(*this).end().remove_const_();
    }

    constexpr const_iterator cbegin() const noexcept
    {
        return begin();
    }

    constexpr const_iterator cend() const noexcept
    {
        return end();
    }

    constexpr auto rbegin() noexcept
    {
        return reverse_iterator{end()};
    }

    constexpr auto rend() noexcept
    {
        return reverse_iterator{begin()};
    }

    constexpr auto rbegin() const noexcept
    {
        return const_reverse_iterator{end()};
    }

    constexpr auto rend() const noexcept
    {
        return const_reverse_iterator{begin()};
    }

    constexpr auto crbegin() const noexcept
    {
        return const_reverse_iterator{end()};
    }

    constexpr auto crend() const noexcept
    {
        return const_reverse_iterator{begin()};
    }

  private:
    // case 1           case 2            case 3           case 4
    // A1 B1 → A2 B2   A1       A2       A1 B1   A2       A1       A2 B2
    // |        |       |        |        |    ↘ |        |     ↗ |
    // C1    → C2      B1    → B2       C1      B2       B1       C2
    // |        |       |        |        |    ↘ |        |     ↗ |
    // D1       D2      D1 C1 → C2 D2    D1      C1 D1    C1 D1   D2
    // case 1 4: back
    // case 2 3: front

    // 负责分配块数组
    // 构造和扩容时都可以使用
    struct ctrl_alloc_
    {
        deque &d;
        BlockFP block_ctrl_begin_fancy{}; // A
        Block *block_ctrl_end{};          // D

        // 替换块数组到deque
        // 构造函数专用
        // 对空deque安全
        constexpr void replace_ctrl() const noexcept
        {
            d.block_ctrl_begin_fancy_ = block_ctrl_begin_fancy;
            d.block_ctrl_end_ = block_ctrl_end;
            d.block_alloc_begin_ = deque_detail::to_address(block_ctrl_begin_fancy);
            d.block_alloc_end_ = d.block_alloc_begin_;
            d.block_elem_begin_ = d.block_alloc_begin_;
            d.block_elem_end_ = d.block_alloc_begin_;
        }

        // 扩容时，back为插入元素的方向
        // 对空deque安全
        constexpr void replace_ctrl_back() const noexcept
        {
            d.align_elem_alloc_as_ctrl_back_(deque_detail::to_address(block_ctrl_begin_fancy));
            d.dealloc_ctrl_();
            // 注意顺序
            // 从alloc替换回deque
            d.block_ctrl_begin_fancy_ = block_ctrl_begin_fancy;
            d.block_ctrl_end_ = block_ctrl_end;
        }

        constexpr void replace_ctrl_front() const noexcept
        {
            d.align_elem_alloc_as_ctrl_front_(block_ctrl_end);
            d.dealloc_ctrl_();
            // 注意顺序
            // 从alloc替换回deque
            d.block_ctrl_begin_fancy_ = block_ctrl_begin_fancy;
            d.block_ctrl_end_ = block_ctrl_end;
        }

        // 参数是新大小
        constexpr ctrl_alloc_(deque &dq, ::std::size_t const ctrl_size) : d(dq)
        {
            auto const size = (ctrl_size + (::std::size_t(4) - ::std::size_t(1))) / ::std::size_t(4) * ::std::size_t(4);
            block_ctrl_begin_fancy = d.alloc_ctrl_(size);
            block_ctrl_end = deque_detail::to_address(block_ctrl_begin_fancy) + size;
        }
    };

    // 对齐控制块
    // 对齐alloc和ctrl的begin
    constexpr void align_alloc_as_ctrl_back_() noexcept
    {
        ::std::copy(block_alloc_begin_, block_alloc_end_, block_ctrl_begin_());
        auto const block_size = block_alloc_size_();
        block_alloc_begin_ = block_ctrl_begin_();
        block_alloc_end_ = block_ctrl_begin_() + block_size;
    }

    // 对齐控制块
    // 对齐alloc和ctrl的end
    constexpr void align_alloc_as_ctrl_front_() noexcept
    {
        ::std::copy_backward(block_alloc_begin_, block_alloc_end_, block_ctrl_end_);
        auto const block_size = block_alloc_size_();
        block_alloc_end_ = block_ctrl_end_;
        block_alloc_begin_ = block_ctrl_end_ - block_size;
    }

    // 对齐控制块
    // 对齐elem和alloc的begin
    constexpr void align_elem_as_alloc_back_() noexcept
    {
        ::std::rotate(block_alloc_begin_, block_elem_begin_, block_elem_end_);
        auto const block_size = block_elem_size_();
        block_elem_begin_ = block_alloc_begin_;
        block_elem_end_ = block_alloc_begin_ + block_size;
    }

    // 对齐控制块
    // 对齐elem和alloc的end
    constexpr void align_elem_as_alloc_front_() noexcept
    {
        ::std::rotate(block_elem_begin_, block_elem_end_, block_alloc_end_);
        auto const block_size = block_elem_size_();
        block_elem_end_ = block_alloc_end_;
        block_elem_begin_ = block_alloc_end_ - block_size;
    }

    // ctrl_begin可以是自己或者新ctrl的
    // 对齐控制块所有指针
    constexpr void align_elem_alloc_as_ctrl_back_(Block *const ctrl_begin) noexcept
    {
        align_elem_as_alloc_back_();
        auto const alloc_block_size = block_alloc_size_();
        auto const elem_block_size = block_elem_size_();
        ::std::copy(block_alloc_begin_, block_alloc_end_, ctrl_begin);
        block_alloc_begin_ = ctrl_begin;
        block_alloc_end_ = ctrl_begin + alloc_block_size;
        block_elem_begin_ = ctrl_begin;
        block_elem_end_ = ctrl_begin + elem_block_size;
    }

    // ctrl_end可以是自己或者新ctrl的
    // 对齐控制块所有指针
    constexpr void align_elem_alloc_as_ctrl_front_(Block *const ctrl_end) noexcept
    {
        align_elem_as_alloc_front_();
        auto const alloc_block_size = block_alloc_size_();
        auto const elem_block_size = block_elem_size_();
        ::std::copy_backward(block_alloc_begin_, block_alloc_end_, ctrl_end);
        block_alloc_end_ = ctrl_end;
        block_alloc_begin_ = ctrl_end - alloc_block_size;
        block_elem_end_ = ctrl_end;
        block_elem_begin_ = ctrl_end - elem_block_size;
    }

    // 向前分配新block，需要block_size小于等于(block_alloc_begin -
    // block_ctrl_begin) 且不block_alloc_X不是空指针
    constexpr void extent_block_front_uncond_(::std::size_t const block_size)
    {
        assert(block_alloc_begin_ != block_ctrl_begin_());
        assert(block_alloc_begin_ != nullptr);
        auto const target_end = block_alloc_begin_ - block_size;
        for (auto target_begin = block_alloc_begin_; target_begin != target_end;)
        {
            --target_begin;
            *target_begin = alloc_block_();
            block_alloc_begin_ = target_begin;
        }
    }

    // 向后分配新block，需要block_size小于等于(block_ctrl_end - block_alloc_end)
    // 且不block_alloc_X不是空指针
    constexpr void extent_block_back_uncond_(::std::size_t const block_size)
    {
        assert(block_alloc_end_ != block_ctrl_end_);
        assert(block_alloc_end_ != nullptr);
        auto const target_end = block_alloc_end_ + block_size;
        for (auto target_begin = block_alloc_end_; target_begin != target_end; ++target_begin)
        {
            *target_begin = alloc_block_();
            block_alloc_end_ = target_begin;
        }
        block_alloc_end_ = target_end;
    }

    // 向back扩展
    // 对空deque安全
    constexpr void reserve_back_(::std::size_t const add_elem_size)
    {
        // 计算现有头尾是否够用
        // 头部块的cap
        auto const head_block_cap = (block_elem_begin_ - block_alloc_begin_) * deque_detail::block_elements_v<T>;
        // 尾部块的cap
        auto const tail_block_cap = (block_alloc_end_ - block_elem_end_) * deque_detail::block_elements_v<T>;
        // 尾块的已使用大小
        auto const tail_cap = elem_end_last_ - elem_end_end_ + ::std::size_t(0);
        // non_move_cap为尾部-尾部已用，不移动块时cap
        auto const non_move_cap = tail_block_cap + tail_cap;
        // 首先如果cap足够，则不需要分配新block
        if (non_move_cap >= add_elem_size)
        {
            return;
        }
        // move_cap为头部+尾部-尾部已用，移动已分配块的cap
        auto const move_cap = head_block_cap + non_move_cap;
        // 如果move_cap够则移动
        if (move_cap >= add_elem_size)
        {
            align_elem_as_alloc_back_();
            return;
        }
        // 计算需要分配多少块数组，无论接下来是什么逻辑都直接使用它
        auto const add_block_size = (add_elem_size - move_cap + deque_detail::block_elements_v<T> - ::std::size_t(1)) /
                                    deque_detail::block_elements_v<T>;
        // 获得目前控制块容许容量
        auto const ctrl_cap = ((block_alloc_begin_ - block_ctrl_begin_()) + (block_ctrl_end_ - block_alloc_end_)) *
                                  deque_detail::block_elements_v<T> +
                              move_cap;
        // 如果容许容量足够，那么移动alloc
        if (ctrl_cap >= add_elem_size)
        {
            align_elem_alloc_as_ctrl_back_(block_ctrl_begin_());
        }
        else
        {
            // 否则扩展控制块
            ctrl_alloc_ const ctrl{*this, block_alloc_size_() + add_block_size}; // may throw
            ctrl.replace_ctrl_back();
        }
        extent_block_back_uncond_(add_block_size);
    }

    // 向back扩展
    // 对空deque安全
    constexpr void reserve_one_back_()
    {
#if 1
        reserve_back_(::std::size_t(1));
#else
        if (block_alloc_end_ != block_elem_end_)
        {
            return;
        }
        if (block_elem_begin_ != block_alloc_begin_)
        {
            align_elem_as_alloc_back_();
            return;
        }
        if ((block_alloc_begin_ - block_ctrl_begin_()) + (block_ctrl_end_ - block_alloc_end_) != ::std::size_t(0))
        {
            align_elem_alloc_as_ctrl_back_(block_ctrl_begin_());
        }
        else
        {
            // 否则扩展控制块
            ctrl_alloc_ const ctrl{*this, block_alloc_size_() + ::std::size_t(1)}; // may throw
            ctrl.replace_ctrl_back();
        }
        extent_block_back_uncond_(::std::size_t(1));
#endif
    }

    // 从front扩展block，空deque安全
    constexpr void reserve_front_(::std::size_t const add_elem_size)
    {
        // 计算现有头尾是否够用
        // 头部块的cap
        auto const head_block_alloc_cap = (block_elem_begin_ - block_alloc_begin_) * deque_detail::block_elements_v<T>;
        // 尾部块的cap
        auto const tail_block_alloc_cap = (block_alloc_end_ - block_elem_end_) * deque_detail::block_elements_v<T>;
        // 头块的已使用大小
        auto const head_cap = elem_begin_begin_ - elem_begin_first_ + ::std::size_t(0);
        // non_move_cap为头部-头部已用，不移动块时cap
        auto const non_move_cap = head_block_alloc_cap + head_cap;
        // 首先如果cap足够，则不需要分配新block
        if (non_move_cap >= add_elem_size)
        {
            return;
        }
        // move_cap为头部-头部已用+尾部，移动已分配块的cap
        auto const move_cap = non_move_cap + tail_block_alloc_cap;
        // 如果move_cap够则移动
        if (move_cap >= add_elem_size)
        {
            align_elem_as_alloc_front_();
            return;
        }
        // 计算需要分配多少块数组，无论接下来是什么逻辑都直接使用它
        auto const add_block_size = (add_elem_size - move_cap + deque_detail::block_elements_v<T> - ::std::size_t(1)) /
                                    deque_detail::block_elements_v<T>;
        // 获得目前控制块容许容量
        auto const ctrl_cap = ((block_alloc_begin_ - block_ctrl_begin_()) + (block_ctrl_end_ - block_alloc_end_)) *
                                  deque_detail::block_elements_v<T> +
                              move_cap;
        if (ctrl_cap >= add_elem_size)
        {
            align_elem_alloc_as_ctrl_front_(block_ctrl_end_);
        }
        else
        {
            // 否则扩展控制块
            ctrl_alloc_ const ctrl{*this, block_alloc_size_() + add_block_size}; // may throw
            ctrl.replace_ctrl_front();
        }
        // 必须最后执行
        extent_block_front_uncond_(add_block_size);
    }

    // 向back扩展
    // 对空deque安全
    constexpr void reserve_one_front_()
    {
#if 1
        reserve_front_(::std::size_t(1));
#else
        if (block_elem_begin_ != block_alloc_begin_)
        {
            return;
        }
        if (block_alloc_end_ != block_elem_end_)
        {
            align_elem_as_alloc_front_();
            return;
        }
        if ((block_alloc_begin_ - block_ctrl_begin_()) + (block_ctrl_end_ - block_alloc_end_) != ::std::size_t(0))
        {
            align_elem_alloc_as_ctrl_front_(block_ctrl_end_);
        }
        else
        {
            // 否则扩展控制块
            ctrl_alloc_ const ctrl{*this, block_alloc_size_() + ::std::size_t(1)}; // may throw
            ctrl.replace_ctrl_front();
        }
        extent_block_front_uncond_(::std::size_t(1));
#endif
    }

    struct construct_guard_
    {
      private:
        deque *d;

      public:
        constexpr construct_guard_(deque *dp) noexcept : d(dp)
        {
        }

        constexpr void release() noexcept
        {
            d = nullptr;
        }

        constexpr ~construct_guard_()
        {
            if (d)
            {
                d->destroy_();
            }
        }
    };

    // 构造函数和赋值的辅助函数
    // 调用后可直接填充元素
    constexpr void extent_block_(::std::size_t const new_block_size)
    {
        if (new_block_size != ::std::size_t(0))
        {
            auto const ctrl_block_size = block_ctrl_size_();
            auto const alloc_block_size = block_alloc_size_();
            if (ctrl_block_size == ::std::size_t(0))
            {
                ctrl_alloc_ const ctrl(*this, new_block_size); // may throw
                ctrl.replace_ctrl();
                extent_block_back_uncond_(new_block_size); // may throw
                return;
            }
            if (alloc_block_size >= new_block_size)
            {
                return;
            }
            if (ctrl_block_size < new_block_size)
            {
                ctrl_alloc_ const ctrl(*this, new_block_size); // may throw
                ctrl.replace_ctrl_back();
            }
            else
            {
                align_alloc_as_ctrl_back_();
            }
            extent_block_back_uncond_(new_block_size - alloc_block_size); // may throw
        }
    }

    // 构造函数和复制赋值的辅助函数，调用前必须分配内存，以及用于构造时使用guard
    template <bool move = false>
    constexpr void copy_(const_buckets_type const other, ::std::size_t const block_size)
    {
        if (block_size)
        {
            // 此时最为特殊，因为只有一个有效快时，可以从头部生长也可以从尾部生长
            // 这里选择按头部生长简化代码
            auto const elem_size = other.elem_begin_end_ - other.elem_begin_begin_;
            auto const first = *block_elem_end_;
            auto const last = first + deque_detail::block_elements_v<T>;
            auto const begin = last - elem_size;
            if constexpr (move)
            {
                deque_detail::uninitialized_move(allocator_, other.elem_begin_begin_, other.elem_begin_end_, begin,
                                                 ::std::unreachable_sentinel);
            }
            else
            {
                deque_detail::uninitialized_copy(allocator_, other.elem_begin_begin_, other.elem_begin_end_, begin,
                                                 ::std::unreachable_sentinel);
            }
            elem_begin_(begin, last, first);
            elem_end_(begin, last, last);
            ++block_elem_end_;
        }
        if (block_size > ::std::size_t(2))
        {
            for (auto const block_begin : ::std::ranges::subrange{other.block_elem_begin_ + ::std::size_t(1),
                                                                  other.block_elem_end_ - ::std::size_t(1)})
            {
                auto const begin = *block_elem_end_;
                auto const src_begin = block_begin;
                if constexpr (move)
                {
                    deque_detail::uninitialized_move(allocator_, src_begin,
                                                     src_begin + deque_detail::block_elements_v<T>, begin,
                                                     ::std::unreachable_sentinel);
                }
                else
                {
                    deque_detail::uninitialized_copy(allocator_, src_begin,
                                                     src_begin + deque_detail::block_elements_v<T>, begin,
                                                     ::std::unreachable_sentinel);
                }
                elem_end_(begin, begin + deque_detail::block_elements_v<T>, elem_end_last_);
                ++block_elem_end_;
            }
            elem_end_last_ = elem_end_end_;
        }
        if (block_size > ::std::size_t(1))
        {
            auto const begin = *block_elem_end_;
            if constexpr (move)
            {
                deque_detail::uninitialized_move(allocator_, other.elem_end_begin_, other.elem_end_end_, begin,
                                                 ::std::unreachable_sentinel);
            }
            else
            {
                deque_detail::uninitialized_copy(allocator_, other.elem_end_begin_, other.elem_end_end_, begin,
                                                 ::std::unreachable_sentinel);
            }
            elem_end_(begin, begin + (other.elem_end_end_ - other.elem_end_begin_),
                      begin + deque_detail::block_elements_v<T>);
            ++block_elem_end_;
        }
    }

  public:
    constexpr deque() noexcept(::std::is_nothrow_default_constructible_v<Alloc>)
        requires ::std::default_initializable<Alloc>
    = default;

    explicit constexpr deque(Alloc const &alloc) noexcept(::std::is_nothrow_copy_constructible_v<Alloc>)
        : allocator_(alloc)
    {
    }

  private:
    // 万能构造
    // 使用count、count和T、或者随机访问迭代器进行构造
    // 注意异常安全，需要调用者使用guard，并且分配好足够多内存
    template <typename... Ts>
    constexpr void construct_(::std::size_t const full_blocks, ::std::size_t const rem_elems, Ts &&...ts)
    {
        // 由于析构优先考虑elem_begin，因此必须独立构造elem_begin
        if (full_blocks)
        {
            auto const begin = ::std::to_address(*block_elem_end_);
            auto const end = begin + deque_detail::block_elements_v<T>;
            if constexpr (sizeof...(Ts) == ::std::size_t(0))
            {
                deque_detail::uninitialized_value_construct(allocator_, begin, end);
                elem_begin_(begin, end, begin);
            }
            else if constexpr (sizeof...(Ts) == ::std::size_t(1))
            {
                deque_detail::uninitialized_fill(allocator_, begin, end, ts...);
                elem_begin_(begin, end, begin);
            }
            else if constexpr (sizeof...(Ts) == ::std::size_t(2))
            {
                auto const pair = deque_detail::get_iter_pair(ts...);
                auto const target_end =
                    pair.src_begin +
                    static_cast<::std::iter_difference_t<decltype(pair.src_begin)>>(deque_detail::block_elements_v<T>);
                deque_detail::uninitialized_copy(allocator_, pair.src_begin, target_end, begin,
                                                 ::std::unreachable_sentinel);
                pair.src_begin = target_end;
                elem_begin_(begin, end, begin);
            }
            else
            {
                static_assert(false);
            }
            elem_end_(begin, end, end);
            ++block_elem_end_;
        }
        if (full_blocks > ::std::size_t(1))
        {
            for (auto i = ::std::size_t(0); i != full_blocks - ::std::size_t(1); ++i)
            {
                auto const begin = ::std::to_address(*block_elem_end_);
                auto const end = begin + deque_detail::block_elements_v<T>;
                if constexpr (sizeof...(Ts) == ::std::size_t(0))
                {
                    deque_detail::uninitialized_value_construct(allocator_, begin, end);
                    elem_end_(begin, end, elem_end_last_);
                }
                else if constexpr (sizeof...(Ts) == ::std::size_t(1))
                {
                    deque_detail::uninitialized_fill(allocator_, begin, end, ts...);
                    elem_end_(begin, end, elem_end_last_);
                }
                else if constexpr (sizeof...(Ts) == ::std::size_t(2))
                {
                    auto const pair = deque_detail::get_iter_pair(ts...);
                    auto const target_end =
                        pair.src_begin + static_cast<::std::iter_difference_t<decltype(pair.src_begin)>>(
                                             deque_detail::block_elements_v<T>);
                    deque_detail::uninitialized_copy(allocator_, pair.src_begin, target_end, begin,
                                                     ::std::unreachable_sentinel);
                    pair.src_begin = target_end;
                    elem_end_(begin, end, elem_end_last_);
                }
                else
                {
                    static_assert(false);
                }
                ++block_elem_end_;
            }
            elem_end_last_ = elem_end_end_;
        }
        if (rem_elems)
        {
            auto const begin = ::std::to_address(*block_elem_end_);
            auto const end = begin + rem_elems;
            if constexpr (sizeof...(Ts) == ::std::size_t(0))
            {
                deque_detail::uninitialized_value_construct(allocator_, begin, end);
                elem_end_(begin, end, begin + deque_detail::block_elements_v<T>);
            }
            else if constexpr (sizeof...(Ts) == ::std::size_t(1))
            {
                deque_detail::uninitialized_fill(allocator_, begin, end, ts...);
                elem_end_(begin, end, begin + deque_detail::block_elements_v<T>);
            }
            else if constexpr (sizeof...(Ts) == ::std::size_t(2))
            {
                auto const pair = deque_detail::get_iter_pair(ts...);
                deque_detail::uninitialized_copy(allocator_, pair.src_begin, pair.src_end, begin,
                                                 ::std::unreachable_sentinel);
                elem_end_(begin, end, begin + deque_detail::block_elements_v<T>);
            }
            else
            {
                static_assert(false);
            }
            if (full_blocks == ::std::size_t(0)) // 注意
            {
                elem_begin_(begin, end, begin);
            }
            ++block_elem_end_;
        }
    }

    // 参考emplace_front
    template <typename... V>
    constexpr T &emplace_back_pre_(V &&...v)
    {
        auto const end = elem_end_end_;
        atraits_t_::construct(allocator_, end, ::std::forward<V>(v)...); // may throw
        elem_end_end_ = end + ::std::size_t(1);
        // 修正elem_begin
        if (block_elem_size_() == ::std::size_t(1))
        {
            elem_begin_end_ = end + ::std::size_t(1);
        }
        return *end;
    }

    // 参考emplace_front
    template <typename... V>
    constexpr T &emplace_back_post_(V &&...v)
    {
        auto const begin = ::std::to_address(*block_elem_end_);
        atraits_t_::construct(allocator_, begin, ::std::forward<V>(v)...); // may throw
        elem_end_(begin, begin + ::std::size_t(1), begin + deque_detail::block_elements_v<T>);
        ++block_elem_end_;
        // 修正elem_begin
        if (block_elem_size_() == ::std::size_t(1))
        {
            elem_begin_(begin, begin + ::std::size_t(1), begin);
        }
        return *begin;
    }

  public:
    template <typename... V>
    constexpr T &emplace_back(V &&...v)
    {
        if (elem_end_end_ != elem_end_last_)
        {
            return emplace_back_pre_(::std::forward<V>(v)...);
        }
        else
        {
            reserve_one_back_();
            return emplace_back_post_(::std::forward<V>(v)...);
        }
    }

    explicit deque(size_type const count, Alloc const &alloc = Alloc()) : allocator_(alloc)
    {
        assert(allocator_ == alloc);
        auto const res = deque_detail::calc_cap<T>(static_cast<::std::size_t>(count));
        construct_guard_ guard(this);
        extent_block_(res.block_size);
        construct_(res.full_blocks, res.rem_elems);
        guard.release();
    }

    // TRANSITION: https://github.com/microsoft/STL/pull/4254
    // TRANSITION: CWG2369, MSVC
    template <typename = ::std::enable_if_t<deque_detail::mini_alloc<Alloc>>>
    constexpr deque(size_type const count, T const &value, Alloc const &alloc = Alloc()) : allocator_(alloc)
    {
        assert(allocator_ == alloc);
        auto const res = deque_detail::calc_cap<T>(static_cast<::std::size_t>(count));
        construct_guard_ guard(this);
        extent_block_(res.block_size);
        construct_(res.full_blocks, res.rem_elems, value);
        guard.release();
    }

  private:
    template <typename U, typename V>
        requires ::std::input_iterator<::std::remove_reference_t<U>>
    constexpr void from_range_noguard_(U &&first, V &&last)
    {
        append_range_noguard_(first, last);
    }

    template <typename U>
        requires ::std::random_access_iterator<::std::remove_reference_t<U>>
    constexpr void from_range_noguard_(U &&first, U &&last)
    {
        if (first != last)
        {
            auto const res = deque_detail::calc_cap<T>(static_cast<::std::size_t>(last - first));
            extent_block_(res.block_size);
            construct_(res.full_blocks, res.rem_elems, ::std::move(first), ::std::move(last));
        }
    }

    constexpr void from_range_noguard_(iterator &first, iterator &last)
    {
        if (first != last)
        {
            if (first.block_elem_curr_ == last.block_elem_curr_)
            {
                buckets_type bucket{first.block_elem_curr_, last.block_elem_curr_ + ::std::size_t(1),
                                    first.elem_curr_,       last.elem_curr_,
                                    last.elem_begin_,       last.elem_begin_};
                auto const block_size = bucket.size();
                extent_block_(block_size);
                copy_(bucket, block_size);
            }
            else
            {
                buckets_type bucket{first.block_elem_curr_, last.block_elem_curr_ + ::std::size_t(1),
                                    first.elem_curr_,       first.elem_begin_ + deque_detail::block_elements_v<T>,
                                    last.elem_begin_,       last.elem_curr_};
                auto const block_size = bucket.size();
                extent_block_(block_size);
                copy_(bucket, block_size);
            }
        }
    }

    constexpr void from_range_noguard_(iterator &&first, iterator &&last)
    {
        return from_range_noguard_(first, last);
    }

    template <typename R>
    constexpr void from_range_noguard_(R &&rg)
    {
        from_range_noguard_(::std::ranges::begin(rg), ::std::ranges::end(rg));
    }

  public:
    template <::std::input_iterator U>
    constexpr deque(U first, U last, Alloc const &alloc = Alloc()) : allocator_(alloc)
    {
        assert(allocator_ == alloc);
        construct_guard_ guard(this);
        from_range_noguard_(::std::move(first), ::std::move(last));
        guard.release();
    }

#if defined(__cpp_lib_containers_ranges)
    template <::std::ranges::input_range R> // _Container_compatible_range<_Ty>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, T>
    constexpr deque(::std::from_range_t, R &&rg, Alloc const &alloc = Alloc()) : allocator_(alloc)
    {
        assert(allocator_ == alloc);
        construct_guard_ guard(this);
        from_range_noguard_(rg);
        guard.release();
    }
#endif

    // 复制构造采取按结构复制的方法
    constexpr deque(deque const &other)
        : allocator_(atraits_t_::select_on_container_copy_construction(other.allocator_))
    {
        if (!other.empty())
        {
            construct_guard_ guard(this);
            auto const block_size = other.block_elem_size_();
            extent_block_(block_size);
            copy_(other.buckets(), block_size);
            guard.release();
        }
    }

    constexpr deque(deque const &other, ::std::type_identity_t<Alloc> const &alloc) : allocator_(alloc)
    {
        assert(allocator_ == alloc);
        if (!other.empty())
        {
            construct_guard_ guard(this);
            auto const block_size = other.block_elem_size_();
            extent_block_(block_size);
            copy_(other.buckets(), block_size);
            guard.release();
        }
    }

    constexpr deque(deque &&other) noexcept(::std::is_nothrow_copy_constructible_v<Alloc>)
        : allocator_(::std::move(other.allocator_))
    {
        assert(allocator_ == other.allocator_);
        other.swap_without_ator_(*this);
    }

    constexpr deque(deque &&other, ::std::type_identity_t<Alloc> const &alloc) noexcept(is_aleq_) : allocator_(alloc)
    {
        if constexpr (is_aleq_)
        {
            other.swap_without_ator_(*this);
        }
        else
        {
            assert(allocator_ == alloc);

            if (allocator_ == other.allocator_)
            {
                other.swap_without_ator_(*this);
            }
            else
            {
                construct_guard_ guard(this);
                auto const block_size = other.block_elem_size_();
                extent_block_(block_size);
                copy_<true>(other.buckets(), block_size);
                guard.release();
            }
        }
    }

    constexpr deque(::std::initializer_list<T> const ilist, Alloc const &alloc = Alloc()) : allocator_(alloc)
    {
        assert(allocator_ == alloc);
        if (ilist.size())
        {
            construct_guard_ guard(this);
            from_range_noguard_(ilist.begin(), ilist.end());
            guard.release();
        }
    }

    constexpr deque &operator=(deque const &other)
    {
        if (this != ::std::addressof(other))
        {
            if constexpr (is_pocca_)
            {
                clear_full_();
                if constexpr (is_aleq_)
                {
                    allocator_ = other.allocator_;
                }
                else
                {
                    if (allocator_ != other.allocator_)
                    {
                        allocator_ = other.allocator_;
                    }
                }
            }
            else
            {
                clear();
            }
            if (!other.empty())
            {
                auto const block_size = other.block_elem_size_();
                extent_block_(block_size);
                copy_(other.buckets(), block_size);
            }
        }
        return *this;
    }

    constexpr deque &operator=(::std::initializer_list<T> ilist)
    {
        clear();
        if (ilist.size())
        {
            auto const res = deque_detail::calc_cap<T>(ilist.size());
            extent_block_(res.block_size);
            construct_(res.full_blocks, res.rem_elems, ::std::begin(ilist), ::std::end(ilist));
        }
        return *this;
    }

    constexpr deque &operator=(deque &&other) noexcept(is_aleq_ || is_pocma_)
    {
        if (this == ::std::addressof(other))
        {
            return *this;
        }
        if constexpr (is_pocma_)
        {
            if constexpr (is_aleq_)
            {
                allocator_ = ::std::move(other.allocator_);
            }
            else
            {
                if (allocator_ == other.allocator_)
                {
                    clear();
                }
                else
                {
                    clear_full_();
                    allocator_ = ::std::move(other.allocator_);
                }
            }
            other.swap_without_ator_(*this);
        }
        else
        {
            clear();
            if (allocator_ == other.allocator_)
            {
                other.swap_without_ator_(*this);
            }
            else
            {
                if (!other.empty())
                {
                    auto const block_size = other.block_elem_size_();
                    extent_block_(block_size);
                    copy_<true>(other.buckets(), block_size);
                }
            }
        }
        return *this;
    }

    constexpr void assign_range(deque &&d)
    {
        *this = ::std::move(d);
    }

    constexpr void assign_range(deque const &d)
    {
        *this = d;
    }

    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, T>
    constexpr void assign_range(R &&rg)
    {
        clear();
        from_range_noguard_(::std::forward<R>(rg));
    }

    constexpr void assign(size_type const count, T const &value)
    {
        clear();
        if (count)
        {
            auto const res = deque_detail::calc_cap<T>(static_cast<::std::size_t>(count));
            extent_block_(res.block_size);
            construct_(res.full_blocks, res.rem_elems, value);
        }
        /*
        assign_range(::std::ranges::views::repeat(value, count));
        */
    }

    template <typename U, typename V>
        requires ::std::input_iterator<::std::remove_reference_t<U>>
    constexpr void assign(U first, V last)
    {
        clear();
        from_range_noguard_(::std::move(first), ::std::move(last));
    }

    constexpr void assign(::std::initializer_list<T> const ilist)
    {
        clear();
        from_range_noguard_(ilist.begin(), ilist.end());
    }

  private:
    // 几乎等于iterator的at，但具有检查和断言
    template <bool throw_exception = false>
    constexpr T &at_impl_(::std::size_t const pos) const noexcept(!throw_exception)
    {
        auto const front_size = static_cast<::std::size_t>(elem_begin_begin_ - elem_begin_first_);
        auto const res = deque_detail::calc_pos<T>(front_size, pos);
        auto const target_block = block_elem_begin_ + res.block_step;
        auto const check_block = target_block < block_elem_end_;
        auto const check_elem = (target_block + ::std::size_t(1) == block_elem_end_)
                                    ? (::std::to_address(*target_block) + res.elem_step < elem_end_end_)
                                    : true;
        if constexpr (throw_exception)
        {
            if (!(check_block && check_elem))
            {
#if defined(__cpp_exceptions)
                throw ::std::out_of_range("bizwen::deque::at");
#else
                ::std::terminate();
#endif
            }
        }
        else
        {
            assert(check_block && check_elem);
        }
        return *((*target_block) + res.elem_step);
    }

    // 首块有空余时使用
    template <typename... V>
    constexpr T &emplace_front_pre_(V &&...v)
    {
        auto const begin = ::std::to_address(elem_begin_begin_ - ::std::size_t(1));
        atraits_t_::construct(allocator_, begin, ::std::forward<V>(v)...); // may throw
        elem_begin_begin_ = begin;
        if (block_elem_size_() == ::std::size_t(1))
        {
#if __has_cpp_attribute(assume)
            [[assume(begin + ::std::size_t(1) == elem_end_begin_)]];
#endif
            elem_end_begin_ = begin;
        }
        return *begin;
    }

    // 首块没有空余，切换到下一个块
    template <typename... V>
    constexpr T &emplace_front_post_(V &&...v)
    {
        auto const block = block_elem_begin_ - ::std::size_t(1);
        auto const first = ::std::to_address(*block);
        auto const end = first + deque_detail::block_elements_v<T>;
        atraits_t_::construct(allocator_, end - ::std::size_t(1), ::std::forward<V>(v)...); // may throw
        elem_begin_(end - ::std::size_t(1), end, first);
#if __has_cpp_attribute(assume)
        [[assume(block + ::std::size_t(1) == block_elem_begin_)]];
#endif
        --block_elem_begin_;
        // 修正elem_end
        if (block_elem_size_() == ::std::size_t(1))
        {
            elem_end_(end - ::std::size_t(1), end, end);
        }
        return *(end - ::std::size_t(1));
    }

  public:
    template <typename... V>
    constexpr T &emplace_front(V &&...v)
    {
        if (elem_begin_begin_ != elem_begin_first_)
        {
            return emplace_front_pre_(::std::forward<V>(v)...);
        }
        else
        {
            reserve_one_front_();
            return emplace_front_post_(::std::forward<V>(v)...);
        }
    }

    constexpr T &at(size_type const pos) noexcept
    {
        return at_impl_<true>(pos);
    }

    constexpr T const &at(size_type const pos) const noexcept
    {
        return at_impl_<true>(pos);
    }

    constexpr T &operator[](size_type const pos) noexcept
    {
        return at_impl_(pos);
    }

    constexpr T const &operator[](size_type const pos) const noexcept
    {
        return at_impl_(pos);
    }

    // 不会失败且不移动元素
    constexpr void shrink_to_fit() noexcept
    {
        if (block_alloc_size_() != ::std::size_t(0)) // 保证fill_block_alloc_end
        {
            dealloc_block_range_(block_alloc_begin_, block_elem_begin_);
            block_alloc_begin_ = block_elem_begin_;
            dealloc_block_range_(block_elem_end_, block_alloc_end_);
            block_alloc_end_ = block_elem_end_;
        }
    }

    constexpr void push_back(T const &t)
    {
        emplace_back(t);
    }

    constexpr void push_back(T &&t)
    {
        emplace_back(::std::move(t));
    }

    constexpr void push_front(T const &value)
    {
        emplace_front(value);
    }

    constexpr void push_front(T &&value)
    {
        emplace_front(::std::move(value));
    }

    // 该函数调用后如果容器大小为0，则使得elem_begin/end为nullptr
    // 这是emplace_back的先决条件
    constexpr void pop_back() noexcept
    {
        assert(!empty());
        --elem_end_end_;
        atraits_t_::destroy(allocator_, elem_end_end_);
        if (elem_end_end_ == elem_end_begin_)
        {
            --block_elem_end_;
            auto const block_size = block_elem_size_();
            if (block_size == ::std::size_t(1))
            {
                elem_end_(elem_begin_begin_, elem_begin_end_, elem_begin_end_);
            }
            else if (block_size)
            {
                auto const begin = ::std::to_address(*(block_elem_end_ - ::std::size_t(1)));
                auto const last = begin + deque_detail::block_elements_v<T>;
                elem_end_(begin, last, last);
            }
            else
            {
                elem_begin_(nullptr, nullptr, nullptr);
                elem_end_(nullptr, nullptr, nullptr);
            }
        }
        else if (block_elem_size_() == ::std::size_t(1))
        {
            --elem_begin_end_;
        }
    }

    // 参考pop_back
    constexpr void pop_front() noexcept
    {
        assert(!empty());
        atraits_t_::destroy(allocator_, elem_begin_begin_);
        ++elem_begin_begin_;
        if (elem_begin_end_ == elem_begin_begin_)
        {
            ++block_elem_begin_;
            auto const block_size = block_elem_size_();
            // 注意，如果就剩最后一个block，那么应该采用end的位置而不是计算得到
            if (block_size == ::std::size_t(1))
            {
                elem_begin_(elem_end_begin_, elem_end_end_, elem_end_begin_);
            }
            else if (block_size)
            {
                auto const begin = ::std::to_address(*block_elem_begin_);
                auto const end = begin + deque_detail::block_elements_v<T>;
                elem_begin_(begin, end, begin);
            }
            else
            {
                elem_begin_(nullptr, nullptr, nullptr);
                elem_end_(nullptr, nullptr, nullptr);
            }
        }
        else if (block_elem_size_() == ::std::size_t(1))
        {
            ++elem_end_begin_;
        }
    }

    constexpr T &front() noexcept
    {
        assert(!empty());
        return *(elem_begin_begin_);
    }

    constexpr T &back() noexcept
    {
        assert(!empty());
        return *(elem_end_end_ - ::std::size_t(1));
    }

    constexpr T const &front() const noexcept
    {
        assert(!empty());
        return *(elem_begin_begin_);
    }

    constexpr T const &back() const noexcept
    {
        assert(!empty());
        return *(elem_end_end_ - ::std::size_t(1));
    }

  private:
    constexpr void pop_back_n_(::std::size_t const count) noexcept
    {
        for (auto i = ::std::size_t(0); i != count; ++i)
        {
            assert(!empty());
            pop_back();
        }
    }

    constexpr void pop_front_n_(::std::size_t const count) noexcept
    {
        for (auto i = ::std::size_t(0); i != count; ++i)
        {
            assert(!empty());
            pop_front();
        }
    }

    template <bool back>
    struct partial_guard_
    {
        deque *d;
        ::std::size_t const size;

      public:
        constexpr partial_guard_(deque *dp, ::std::size_t const old_size) noexcept : d(dp), size(old_size)
        {
        }

        constexpr void release() noexcept
        {
            d = nullptr;
        }

        constexpr ~partial_guard_()
        {
            if (d != nullptr)
            {
                if constexpr (back)
                {
                    d->resize_shrink_(d->size(), size);
                }
                else
                {
                    d->pop_front_n_(d->size() - size);
                }
            }
        }
    };

    // 用于范围构造，该函数不分配内存
    // 需要在调用前reserve足够大
    template <typename... V>
    constexpr T &emplace_front_noalloc_(V &&...v)
    {
        if (elem_begin_begin_ != elem_begin_first_)
        {
            return emplace_front_pre_(::std::forward<V>(v)...);
        }
        else
        {
            return emplace_front_post_(::std::forward<V>(v)...);
        }
    }

    template <typename U, typename V>
    constexpr void emplace_front_noalloc_range_(U &&first, V &&last)
    {
        for (; first != last;)
        {
            --last;
            emplace_front(*last);
        }
    }

    // 见emplace_front_noalloc
    template <typename... V>
    constexpr T &emplace_back_noalloc_(V &&...v)
    {
        if (elem_end_end_ != elem_end_last_)
        {
            return emplace_back_pre_(::std::forward<V>(v)...);
        }
        else
        {
            return emplace_back_post_(::std::forward<V>(v)...);
        }
    }

    template <typename U, typename V>
    constexpr void emplace_back_noalloc_range_(U &&first, V &&last)
    {
        for (; first != last; ++first)
        {
            emplace_back_noalloc_(*first);
        }
    }

    template <typename U, typename V>
        requires ::std::input_iterator<::std::remove_reference_t<U>>
    constexpr void append_range_noguard_(U &&first, V &&last)
    {
        for (; first != last; ++first)
        {
            emplace_back(*first);
        }
    }

    template <typename U>
        requires ::std::random_access_iterator<::std::remove_reference_t<U>>
    constexpr void append_range_noguard_(U &&first, U &&last)
    {
        reserve_back_(static_cast<::std::size_t>(last - first));
        emplace_back_noalloc_range_(first, last);
    }

    template <typename R>
    constexpr void append_range_noguard_(R &&rg)
    {
        if constexpr (::std::ranges::sized_range<R>)
        {
            if (::std::ranges::empty(rg))
            {
                return;
            }
            reserve_back_(static_cast<::std::size_t>(::std::ranges::size(rg)));
            emplace_back_noalloc_range_(::std::ranges::begin(rg), ::std::ranges::end(rg));
        }
        else
        {
            append_range_noguard_(::std::ranges::begin(rg), ::std::ranges::end(rg));
        }
    }

    template <typename U, typename V>
        requires ::std::input_iterator<::std::remove_reference_t<U>>
    constexpr void prepend_range_noguard_(U &&first, V &&last)
    {
        auto const old_size = size();
        for (; first != last; ++first)
        {
            emplace_front(*first);
        }
        ::std::reverse(begin(), begin() + static_cast<difference_type>(size() - old_size));
    }

    template <typename U>
        requires ::std::bidirectional_iterator<::std::remove_reference_t<U>>
    constexpr void prepend_range_noguard_(U &&first, U &&last)
    {
        for (; first != last;)
        {
            --last;
            emplace_front(*last);
        }
    }

// TRANSITION: EDG不能区分该重载和bidirectional_iterator重载的区别
// https://developercommunity.visualstudio.com/t/IntellisenceEDG-cannot-correctly-determ/10981026
#if !defined(__EDG__)
    template <typename U>
        requires ::std::random_access_iterator<::std::remove_reference_t<U>>
    constexpr void prepend_range_noguard_(U &&first, U &&last)
    {
        reserve_front_(static_cast<::std::size_t>(last - first));
        emplace_front_noalloc_range_(first, last);
    }
#endif

    template <typename R>
    constexpr void prepend_range_noguard_(R &&rg)
    {
        if constexpr (::std::ranges::sized_range<R> && ::std::ranges::bidirectional_range<R> &&
                      ::std::is_same_v<decltype(::std::ranges::begin(rg)), decltype(::std::ranges::end(rg))>)
        {
            if (::std::ranges::empty(rg))
            {
                return;
            }
            reserve_front_(static_cast<::std::size_t>(::std::ranges::size(rg)));
            emplace_front_noalloc_range_(::std::ranges::begin(rg), ::std::ranges::end(rg));
        }
        else if constexpr (::std::ranges::bidirectional_range<R>)
        {
            prepend_range_noguard_(::std::ranges::begin(rg), ::std::ranges::end(rg));
        }
        else if constexpr (::std::ranges::sized_range<R>)
        {
            if (::std::ranges::empty(rg))
            {
                return;
            }
            auto const count = static_cast<::std::size_t>(::std::ranges::size(rg));
            reserve_front_(count);
            emplace_front_noalloc_range_(::std::ranges::begin(rg), ::std::ranges::end(rg));
            ::std::ranges::reverse(begin(), begin() + static_cast<::std::ptrdiff_t>(count));
        }
        else
        {
            prepend_range_noguard_(::std::ranges::begin(rg), ::std::ranges::end(rg));
        }
    }

  public:
    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, T>
    constexpr void append_range(R &&rg)
    {
        partial_guard_<true> guard(this, size());
        append_range_noguard_(::std::forward<R>(rg));
        guard.release();
    }

    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, T>
    constexpr void prepend_range(R &&rg)
    {
        partial_guard_<false> guard(this, size());
        prepend_range_noguard_(::std::forward<R>(rg));
        guard.release();
    }

  private:
    // 收缩专用
    constexpr void resize_shrink_(::std::size_t const old_size, ::std::size_t const new_size) noexcept
    {
        assert(old_size >= new_size);
        if constexpr (::std::is_trivially_destructible_v<T> && is_default_operation_)
        {
            auto const res =
                deque_detail::calc_pos<T>(static_cast<::std::size_t>(elem_begin_begin_ - elem_begin_first_), new_size);
            if (res.block_step == ::std::size_t(0))
            {
                auto const begin = elem_begin_first_;
                elem_end_(elem_begin_begin_, begin + res.elem_step, begin + deque_detail::block_elements_v<T>);
                block_elem_end_ = block_elem_begin_ + ::std::size_t(1);
                elem_begin_(elem_begin_begin_, begin + res.elem_step, begin);
            }
            else
            {
                auto const target_block = block_elem_begin_ + res.block_step;
                auto const begin = *target_block;
                elem_end_(begin, begin + res.elem_step, begin + deque_detail::block_elements_v<T>);
                block_elem_end_ = target_block + ::std::size_t(1);
            }
        }
        else
        {
            pop_back_n_(old_size - new_size);
        }
    }

    template <typename... Ts>
    constexpr void resize_unified_(::std::size_t const new_size, Ts &&...ts)
    {
        if (auto const old_size = size(); new_size < old_size)
        {
            resize_shrink_(old_size, new_size);
        }
        else
        {
            auto const diff = new_size - old_size;
            reserve_back_(diff);
            partial_guard_<true> guard(this, old_size);
            for (auto i = ::std::size_t(0); i != diff; ++i)
            {
                emplace_back_noalloc_(ts...);
            }
            guard.release();
        }
    }

  public:
    // 注意必须调用clear，使得空容器的elem_begin/elem_end都为空指针
    constexpr void resize(size_type const new_size)
    {
        new_size == ::std::size_t(0) ? clear() : resize_unified_(new_size);
    }

    constexpr void resize(size_type const new_size, T const &t)
    {
        new_size == ::std::size_t(0) ? clear() : resize_unified_(new_size, t);
    }

  private:
    // 用于emplace的辅助函数，调用前需要判断方向
    // 该函数将后半部分向后移动1个位置
    // 从最后一个块开始
    constexpr void back_emplace_(Block *const block_curr, T *const elem_curr)
    {
        auto const block_end = block_elem_end_;
        auto const block_size = block_end - block_curr - ::std::size_t(1);
        // 每次移动时留下的空位
        auto last_elem = elem_end_begin_;
        // 先记录尾块块尾位置
        auto end = elem_end_end_;
        // 再emplace_back
        emplace_back_noalloc_(::std::move(back()));
        // 如果大于一个块，那么移动整个尾块
        if (block_size > ::std::size_t(0))
        {
            auto const begin = last_elem;
            ::std::move_backward(begin, end - ::std::size_t(1), end);
        }
        // 移动中间的块
        if (block_size > ::std::size_t(1))
        {
            auto target_block_end = block_end - ::std::size_t(1);
            for (; target_block_end != block_curr + ::std::size_t(1);)
            {
                --target_block_end;
                auto const target_begin = ::std::to_address(*target_block_end);
                auto const target_end = target_begin + deque_detail::block_elements_v<T>;
                *last_elem = ::std::move(*(target_end - ::std::size_t(1)));
                last_elem = target_begin;
                ::std::move_backward(target_begin, target_end - ::std::size_t(1), target_end);
            }
        }
        // 移动插入位置所在的块
        {
            // 如果插入位置就是尾块，那么采纳之前储存的end作为移动使用的end
            if (block_end - ::std::size_t(1) != block_curr)
            {
                // 否则使用计算出来的end
                end = ::std::to_address(*block_curr + deque_detail::block_elements_v<T>);
                // 将当前块的最后一个移动到上一个块的第一个
                *last_elem = ::std::move(*(end - ::std::size_t(1)));
            }
            // 把插入位置所在块整体右移1
            ::std::move_backward(elem_curr, end - ::std::size_t(1), end);
        }
    }

    // 将前半部分向前移动1
    constexpr void front_emplace_(Block *const block_curr, T *const elem_curr)
    {
        auto const block_begin = block_elem_begin_;
        auto const block_size = block_curr - block_begin + ::std::size_t(0);
        // 向前移动后尾部空出来的的后面一个位置
        auto const last_elem_begin = elem_begin_begin_;
        auto last_elem_end = elem_begin_end_;
        emplace_front_noalloc_(::std::move(front()));
        // 如果block_curr是首个块，那么elem_curr就是终点
        if (block_begin == block_curr)
        {
            last_elem_end = elem_curr;
        }
        // 否则之前储存的last_elem_end是终点
        ::std::move(last_elem_begin + ::std::size_t(1), last_elem_end, last_elem_begin);
        if (block_size > ::std::size_t(1))
        {
            auto target_block_begin = block_begin + ::std::size_t(1);
            for (; target_block_begin != block_curr; ++target_block_begin)
            {
                auto const begin = ::std::to_address(*target_block_begin);
                auto const end = begin + deque_detail::block_elements_v<T>;
                *(last_elem_end - ::std::size_t(1)) = ::std::move(*begin);
                last_elem_end = end;
                ::std::move(begin + ::std::size_t(1), end, begin);
            }
        }
        if (block_size > ::std::size_t(0))
        {
            auto const begin = ::std::to_address(*block_curr);
            if (elem_curr != begin)
            {
                *(last_elem_end - ::std::size_t(1)) = ::std::move(*begin);
                ::std::move(begin + ::std::size_t(1), elem_curr, begin);
            }
        }
    }

  public:
    template <typename... Args>
    constexpr iterator emplace(const_iterator const pos, Args &&...args)
    {
        auto const begin_pre = begin();
        auto const end_pre = end();
        if (pos == end_pre)
        {
            emplace_back(::std::forward<Args>(args)...);
            return end() - ::std::ptrdiff_t(1);
        }
        if (pos == begin_pre)
        {
            emplace_front(::std::forward<Args>(args)...);
            return begin();
        }
        // 此时容器一定不为空
        auto const back_diff = end_pre - pos + ::std::size_t(0);
        auto const front_diff = pos - begin_pre + ::std::size_t(0);
        // NB:
        // 如果args是当前容器的元素的引用，那么必须使得该元素先被emplace_back/front后再被移动到正确位置，否则该引用会失效，同时reserve不会导致引用失效
        // 此处逻辑和无分配器版本稍微不一样
        if (back_diff <= front_diff || (block_elem_size_() == ::std::size_t(1) && elem_end_end_ != elem_end_last_))
        {
            reserve_back_(::std::size_t(2));
            emplace_back_noalloc_(::std::forward<Args>(args)...); // 满足标准要求经过A::construct
            // back_emplace向后移动1个元素并插入，因此先reserve以获得一个不失效的pos
            auto new_pos = begin() + static_cast<difference_type>(front_diff);
            back_emplace_(new_pos.block_elem_curr_, new_pos.elem_curr_);
            *new_pos = ::std::move(back());
            pop_back();
            return new_pos;
        }
        else
        {
            reserve_front_(::std::size_t(2));
            emplace_front_noalloc_(::std::forward<Args>(args)...);
            auto new_pos = end() - static_cast<difference_type>(back_diff);
            front_emplace_(new_pos.block_elem_curr_, new_pos.elem_curr_);
            *(--new_pos) = ::std::move(front());
            pop_front();
            return new_pos;
        }
    }

    constexpr iterator insert(const_iterator const pos, T const &value)
    {
        return emplace(pos, value);
    }

    constexpr iterator insert(const_iterator const pos, T &&value)
    {
        return emplace(pos, ::std::move(value));
    }

  private:
    static inline constexpr auto synth_three_way_ = []<class U, class V>(U const &u, V const &v) {
        if constexpr (::std::three_way_comparable_with<U, V>)
        {
            return u <=> v;
        }
        else
        {
            if (u < v)
                return ::std::weak_ordering::less;
            if (v < u)
                return ::std::weak_ordering::greater;
            return ::std::weak_ordering::equivalent;
        }
    };

  public:
    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, T>
    constexpr iterator insert_range(const_iterator const pos, R &&rg)
    {
        auto const begin_pre = begin();
        auto const end_pre = end();
        if (pos == end_pre)
        {
            auto const old_size = size();
            append_range_noguard_(::std::forward<R>(rg));
            return begin() + static_cast<difference_type>(old_size);
        }
        if (pos == begin_pre)
        {
            prepend_range_noguard_(::std::forward<R>(rg));
            return begin();
        }
        auto const front_diff = pos - begin_pre;
        auto const back_diff = end_pre - pos;
        auto const old_size = front_diff + back_diff;
        if (back_diff <= front_diff)
        {
            append_range_noguard_(::std::forward<R>(rg));
            ::std::rotate(begin() + front_diff, begin() + old_size, end());
        }
        else
        {
            prepend_range_noguard_(::std::forward<R>(rg));
            ::std::rotate(begin(), begin() + (static_cast<difference_type>(size()) - old_size), end() - back_diff);
        }
        return begin() + front_diff;
    }

    // 几乎等于insert_range,但是使用迭代器版本以支持input iterator
    template <::std::input_iterator U, typename V>
    constexpr iterator insert(const_iterator const pos, U first, V last)
    {
        if (first == last)
        {
            return pos.remove_const_();
        }
        auto const begin_pre = begin();
        auto const end_pre = end();
        if (pos == end_pre)
        {
            auto const old_size = size();
            partial_guard_<true> guard(this, old_size);
            append_range_noguard_(::std::forward<U>(first), ::std::forward<V>(last));
            guard.release();
            return begin() + static_cast<difference_type>(old_size);
        }
        if (pos == begin_pre)
        {
            auto const old_size = size();
            partial_guard_<false> guard(this, old_size);
            prepend_range_noguard_(::std::forward<U>(first), ::std::forward<V>(last));
            guard.release();
            return begin();
        }
        auto const front_diff = pos - begin_pre;
        auto const back_diff = end_pre - pos;
        auto const old_size = front_diff + back_diff;
        if (back_diff <= front_diff)
        {
            partial_guard_<true> guard(this, old_size);
            append_range_noguard_(first, last);
            guard.release();
            ::std::rotate(begin() + front_diff, begin() + old_size, end());
        }
        else
        {
            partial_guard_<false> guard(this, old_size);
            prepend_range_noguard_(first, last);
            guard.release();
            ::std::rotate(begin(), begin() + (static_cast<difference_type>(size()) - old_size), end() - back_diff);
        }
        return begin() + front_diff;
    }

    constexpr iterator insert(const_iterator const pos, ::std::initializer_list<T> const ilist)
    {
        return insert(pos, ilist.begin(), ilist.end());
    }

    constexpr iterator insert(const_iterator const pos, size_type const count, T const &value)
    {
#if !defined(__cpp_lib_ranges_repeat) || 1
        return insert(pos, deque_detail::repeat_iterator(::std::ptrdiff_t(0), value),
                      deque_detail::repeat_iterator(static_cast<::std::ptrdiff_t>(count), value));
#else
        return insert_range(pos, ::std::ranges::views::repeat(value, count));
#endif
    }

    constexpr bool operator==(deque const &other) const noexcept
    {
        if (auto const s = size(); s != other.size())
        {
            return false;
        }
        else if (s != ::std::size_t(0))
        {
            return ::std::equal(begin(), end(), other.begin());
        }
        return true;
    }

    constexpr auto operator<=>(deque const &other) const noexcept
        requires requires(T const &t, T const &t1) {
            { t < t1 } -> ::std::convertible_to<bool>;
        }
    {
        return ::std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end(), synth_three_way_);
    }

    constexpr iterator erase(const_iterator const pos) noexcept(::std::is_nothrow_move_assignable_v<value_type>)
    {
        auto const begin_pre = begin();
        auto const end_pre = end();
        if (pos == begin_pre)
        {
            pop_front();
            return begin();
        }
        if (pos + ::std::size_t(1) == end_pre)
        {
            pop_back();
            return end();
        }
        auto const back_diff = end_pre - pos;
        auto const front_diff = pos - begin_pre;
        if (back_diff <= front_diff)
        {
            ::std::move((pos + ::std::ptrdiff_t(1)).remove_const_(), end(), pos.remove_const_());
            pop_back();
            return begin() + front_diff;
        }
        else
        {
            ::std::move_backward(begin(), pos.remove_const_(), (pos + ::std::ptrdiff_t(1)).remove_const_());
            pop_front();
            return begin() + front_diff;
        }
    }

    constexpr iterator erase(const_iterator const first,
                             const_iterator const last) noexcept(::std::is_nothrow_move_assignable_v<value_type>)
    {
        if (first == last)
        {
            return first.remove_const_();
        }
        auto const begin_pre = begin();
        auto const end_pre = end();
        if (first == begin_pre)
        {
            pop_front_n_(static_cast<::std::size_t>(last - first));
            return begin();
        }
        if (last == end_pre)
        {
            pop_back_n_(static_cast<::std::size_t>(last - first));
            return end();
        }
        auto const back_diff = end_pre - last;
        auto const front_diff = first - begin_pre;
        if (back_diff <= front_diff)
        {
            ::std::move(last.remove_const_(), end(), first.remove_const_());
            pop_back_n_(static_cast<::std::size_t>(last - first));
            return begin() + front_diff;
        }
        else
        {
            ::std::move_backward(begin(), first.remove_const_(), last.remove_const_());
            pop_front_n_(static_cast<::std::size_t>(last - first));
            return begin() + front_diff;
        }
    }

#if defined(TEST_STD_VER)
    constexpr bool __invariants() const
    {
        return true;
    }
#endif
};

// TRANSITION: CWG2369, MSVC
template <::std::input_iterator U, typename V,
          typename Alloc = ::std::allocator<typename ::std::iterator_traits<U>::value_type>,
          typename = ::std::enable_if_t<deque_detail::mini_alloc<Alloc>>>
deque(U, V, Alloc = Alloc()) -> deque<typename ::std::iterator_traits<U>::value_type, Alloc>;

#if defined(__cpp_lib_containers_ranges)
// TRANSITION: CWG2369, MSVC
template <::std::ranges::input_range R, typename Alloc = ::std::allocator<::std::ranges::range_value_t<R>>,
          typename = ::std::enable_if_t<deque_detail::mini_alloc<Alloc>>>
deque(::std::from_range_t, R &&, Alloc = Alloc()) -> deque<::std::ranges::range_value_t<R>, Alloc>;
#endif

BIZWEN_EXPORT template <typename T, typename Alloc, typename U = T>
inline constexpr auto erase(deque<T, Alloc> &c, U const &value)
{
    auto const it = ::std::remove(c.begin(), c.end(), value);
    auto const r = static_cast<deque<T, Alloc>::size_type>(c.end() - it);
    c.resize(c.size() - r);
    return r;
}

BIZWEN_EXPORT template <typename T, typename Alloc, typename Pred>
inline constexpr auto erase_if(deque<T, Alloc> &c, Pred pred)
{
    auto const it = ::std::remove_if(c.begin(), c.end(), ::std::move(pred));
    auto const r = static_cast<deque<T, Alloc>::size_type>(c.end() - it);
    c.resize(c.size() - r);
    return r;
}

namespace pmr
{
BIZWEN_EXPORT template <typename T>
using deque = deque<T, ::std::pmr::polymorphic_allocator<T>>;
}
} // namespace bizwen

// STL-vNext END

#undef BIZWEN_EXPORT

#endif
