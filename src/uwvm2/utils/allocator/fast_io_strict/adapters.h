#pragma once

#pragma push_macro("UWVM2_FAST_IO_STRICT_INTERNAL_HAS_BUILTIN")
#undef UWVM2_FAST_IO_STRICT_INTERNAL_HAS_BUILTIN
#ifdef __has_builtin
# define UWVM2_FAST_IO_STRICT_INTERNAL_HAS_BUILTIN(...) __has_builtin(__VA_ARGS__)
#else
# define UWVM2_FAST_IO_STRICT_INTERNAL_HAS_BUILTIN(...) 0
#endif

// To make a constexpr allocator, we need ::std::allocator. Because only new expression and
// ::std::allocator<T>::allocate are allowed in constexpr functions. See https://github.com/microsoft/STL/issues/1532
// https://github.com/microsoft/STL/issues/4002 gcc and clang provide constexpr new, but still won't compile.
// ::std::allocator<T> is NOT freestanding.

namespace uwvm2::utils::allocator::fast_io_strict
{

    namespace details
    {

        template <typename alloc>
        concept has_allocate_impl = requires(::std::size_t n) {
            { alloc::allocate(n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_allocate_aligned_impl = requires(::std::size_t n) {
            { alloc::allocate_aligned(n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_allocate_zero_impl = requires(::std::size_t n) {
            { alloc::allocate_zero(n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_allocate_aligned_zero_impl = requires(::std::size_t n) {
            { alloc::allocate_aligned_zero(n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_allocate_conditional_zero_impl = requires(::std::size_t n, bool zero) {
            { alloc::allocate_conditional_zero(n, zero) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_allocate_aligned_conditional_zero_impl = requires(::std::size_t n, bool zero) {
            { alloc::allocate_aligned_conditional_zero(n, n, zero) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_reallocate_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate(p, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_reallocate_aligned_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_aligned(p, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_reallocate_zero_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_zero(p, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_reallocate_aligned_zero_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_aligned_zero(p, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_reallocate_conditional_zero_impl = requires(void* p, ::std::size_t n, bool zero) {
            { alloc::reallocate_conditional_zero(p, n, zero) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_reallocate_aligned_conditional_zero_impl = requires(void* p, ::std::size_t n, bool zero) {
            { alloc::reallocate_aligned_conditional_zero(p, n, n, zero) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_reallocate_n_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_n(p, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_reallocate_aligned_n_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_aligned_n(p, n, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_reallocate_zero_n_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_zero_n(p, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_reallocate_aligned_zero_n_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_aligned_zero_n(p, n, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_reallocate_n_conditional_zero_impl = requires(void* p, ::std::size_t n, bool zero) {
            { alloc::reallocate_n_conditional_zero(p, n, n, zero) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_reallocate_aligned_n_conditional_zero_impl = requires(void* p, ::std::size_t n, bool zero) {
            { alloc::reallocate_aligned_n_conditional_zero(p, n, n, n, zero) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_deallocate_impl = requires(void* p) {
            { alloc::deallocate(p) } -> ::std::same_as<void>;
        };

        template <typename alloc>
        concept has_deallocate_aligned_impl = requires(void* p, ::std::size_t n) {
            { alloc::deallocate_aligned(p, n) } -> ::std::same_as<void>;
        };

        template <typename alloc>
        concept has_deallocate_n_impl = requires(void* p, ::std::size_t n) {
            { alloc::deallocate_n(p, n) } -> ::std::same_as<void>;
        };

        template <typename alloc>
        concept has_deallocate_aligned_n_impl = requires(void* p, ::std::size_t n) {
            { alloc::deallocate_aligned_n(p, n, n) } -> ::std::same_as<void>;
        };

        template <typename alloc>
        concept has_allocate_at_least_impl = requires(::std::size_t n) {
            { alloc::allocate_at_least(n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_allocate_aligned_at_least_impl = requires(::std::size_t n) {
            { alloc::allocate_aligned_at_least(n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_allocate_zero_at_least_impl = requires(::std::size_t n) {
            { alloc::allocate_zero_at_least(n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_allocate_aligned_zero_at_least_impl = requires(::std::size_t n) {
            { alloc::allocate_aligned_zero_at_least(n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_allocate_conditional_zero_at_least_impl = requires(::std::size_t n, bool zero) {
            { alloc::allocate_conditional_zero_at_least(n, zero) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_allocate_aligned_conditional_zero_at_least_impl = requires(::std::size_t n, bool zero) {
            { alloc::allocate_aligned_conditional_zero_at_least(n, n, zero) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_reallocate_at_least_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_at_least(p, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_reallocate_aligned_at_least_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_aligned_at_least(p, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_reallocate_zero_at_least_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_zero_at_least(p, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_reallocate_aligned_zero_at_least_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_aligned_zero_at_least(p, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_reallocate_conditional_zero_at_least_impl = requires(void* p, ::std::size_t n, bool zero) {
            { alloc::reallocate_conditional_zero_at_least(p, n, zero) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_reallocate_aligned_conditional_zero_at_least_impl = requires(void* p, ::std::size_t n, bool zero) {
            { alloc::reallocate_aligned_conditional_zero_at_least(p, n, n, zero) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_reallocate_n_at_least_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_n_at_least(p, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_reallocate_aligned_n_at_least_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_aligned_n_at_least(p, n, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_reallocate_zero_n_at_least_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_zero_n_at_least(p, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_reallocate_aligned_zero_n_at_least_impl = requires(void* p, ::std::size_t n) {
            { alloc::reallocate_aligned_zero_n_at_least(p, n, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_reallocate_n_conditional_zero_at_least_impl = requires(void* p, ::std::size_t n, bool zero) {
            { alloc::reallocate_n_conditional_zero_at_least(p, n, n, zero) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_reallocate_aligned_n_conditional_zero_at_least_impl = requires(void* p, ::std::size_t n, bool zero) {
            { alloc::reallocate_aligned_n_conditional_zero_at_least(p, n, n, n, zero) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_non_empty_handle_type = requires {
            typename alloc::handle_type;
            requires !::std::is_empty_v<typename alloc::handle_type>;
        };

        template <typename alloc>
        concept has_handle_allocate_impl = requires(typename alloc::handle_type handle, ::std::size_t n) {
            { alloc::handle_allocate(handle, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_allocate_aligned_impl = requires(typename alloc::handle_type handle, ::std::size_t n) {
            { alloc::handle_allocate_aligned(handle, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_allocate_zero_impl = requires(typename alloc::handle_type handle, ::std::size_t n) {
            { alloc::handle_allocate_zero(handle, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_allocate_aligned_zero_impl = requires(typename alloc::handle_type handle, ::std::size_t n) {
            { alloc::handle_allocate_aligned_zero(handle, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_allocate_conditional_zero_impl = requires(typename alloc::handle_type handle, ::std::size_t n, bool zero) {
            { alloc::handle_allocate_conditional_zero(handle, n, zero) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_allocate_aligned_conditional_zero_impl = requires(typename alloc::handle_type handle, ::std::size_t n, bool zero) {
            { alloc::handle_allocate_aligned_conditional_zero(handle, n, n, zero) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_reallocate_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate(handle, p, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_reallocate_aligned_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_aligned(handle, p, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_reallocate_zero_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_zero(handle, p, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_reallocate_aligned_zero_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_aligned_zero(handle, p, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_reallocate_conditional_zero_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n, bool zero) {
            { alloc::handle_reallocate_conditional_zero(handle, p, n, zero) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_reallocate_aligned_conditional_zero_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n, bool zero) {
            { alloc::handle_reallocate_aligned_conditional_zero(handle, p, n, n, zero) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_reallocate_n_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_n(handle, p, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_reallocate_aligned_n_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_aligned_n(handle, p, n, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_reallocate_zero_n_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_zero_n(handle, p, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_reallocate_aligned_zero_n_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_aligned_zero_n(handle, p, n, n, n) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_reallocate_n_conditional_zero_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n, bool zero) {
            { alloc::handle_reallocate_n_conditional_zero(handle, p, n, n, zero) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_reallocate_aligned_n_conditional_zero_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n, bool zero) {
            { alloc::handle_reallocate_aligned_n_conditional_zero(handle, p, n, n, n, zero) } -> ::std::same_as<void*>;
        };

        template <typename alloc>
        concept has_handle_allocate_at_least_impl = requires(typename alloc::handle_type handle, ::std::size_t n) {
            { alloc::handle_allocate_at_least(handle, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_allocate_aligned_at_least_impl = requires(typename alloc::handle_type handle, ::std::size_t n) {
            { alloc::handle_allocate_aligned_at_least(handle, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_allocate_zero_at_least_impl = requires(typename alloc::handle_type handle, ::std::size_t n) {
            { alloc::handle_allocate_zero_at_least(handle, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_allocate_aligned_zero_at_least_impl = requires(typename alloc::handle_type handle, ::std::size_t n) {
            { alloc::handle_allocate_aligned_zero_at_least(handle, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_allocate_conditional_zero_at_least_impl = requires(typename alloc::handle_type handle, ::std::size_t n, bool zero) {
            { alloc::handle_allocate_conditional_zero_at_least(handle, n, zero) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_allocate_aligned_conditional_zero_at_least_impl = requires(typename alloc::handle_type handle, ::std::size_t n, bool zero) {
            { alloc::handle_allocate_aligned_conditional_zero_at_least(handle, n, n, zero) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_reallocate_at_least_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_at_least(handle, p, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_reallocate_aligned_at_least_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_aligned_at_least(handle, p, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_reallocate_zero_at_least_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_zero_at_least(handle, p, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_reallocate_aligned_zero_at_least_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_aligned_zero_at_least(handle, p, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_reallocate_conditional_zero_at_least_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n, bool zero) {
            { alloc::handle_reallocate_conditional_zero_at_least(handle, p, n, zero) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_reallocate_aligned_conditional_zero_at_least_impl =
            requires(typename alloc::handle_type handle, void* p, ::std::size_t n, bool zero) {
                { alloc::handle_reallocate_aligned_conditional_zero_at_least(handle, p, n, n, zero) } -> ::std::same_as<::fast_io::allocation_least_result>;
            };

        template <typename alloc>
        concept has_handle_reallocate_n_at_least_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_n_at_least(handle, p, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_reallocate_aligned_n_at_least_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_aligned_n_at_least(handle, p, n, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_reallocate_zero_n_at_least_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_zero_n_at_least(handle, p, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_reallocate_aligned_zero_n_at_least_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_reallocate_aligned_zero_n_at_least(handle, p, n, n, n) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_reallocate_n_conditional_zero_at_least_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n, bool zero) {
            { alloc::handle_reallocate_n_conditional_zero_at_least(handle, p, n, n, zero) } -> ::std::same_as<::fast_io::allocation_least_result>;
        };

        template <typename alloc>
        concept has_handle_reallocate_aligned_n_conditional_zero_at_least_impl =
            requires(typename alloc::handle_type handle, void* p, ::std::size_t n, bool zero) {
                {
                    alloc::handle_reallocate_aligned_n_conditional_zero_at_least(handle, p, n, n, n, zero)
                } -> ::std::same_as<::fast_io::allocation_least_result>;
            };

        template <typename alloc>
        concept has_handle_deallocate_impl = requires(typename alloc::handle_type handle, void* p) {
            { alloc::handle_deallocate(handle, p) } -> ::std::same_as<void>;
        };

        template <typename alloc>
        concept has_handle_deallocate_aligned_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_deallocate_aligned(handle, p, n) } -> ::std::same_as<void>;
        };

        template <typename alloc>
        concept has_handle_deallocate_n_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_deallocate_n(handle, p, n) } -> ::std::same_as<void>;
        };

        template <typename alloc>
        concept has_handle_deallocate_aligned_n_impl = requires(typename alloc::handle_type handle, void* p, ::std::size_t n) {
            { alloc::handle_deallocate_aligned_n(handle, p, n, n) } -> ::std::same_as<void>;
        };

        template <typename alloc>
        concept has_default_alignment_impl = requires(::std::size_t n) { alloc::default_alignment; };

        template <typename alloc>
        inline constexpr ::std::size_t calculate_default_alignment() noexcept
        {
            if constexpr(has_default_alignment_impl<alloc>) { return alloc::default_alignment; }
            else
            {
                return __STDCPP_DEFAULT_NEW_ALIGNMENT__;
            }
        }

        inline constexpr ::std::size_t allocator_compute_aligned_total_size_impl(::std::size_t alignment, ::std::size_t n) noexcept
        {
            constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max()}, sizeofptr{sizeof(void*)}, mxmptr{mxn - sizeofptr};
            if(alignment < sizeofptr) { alignment = sizeofptr; }
            if(alignment > mxmptr) { ::fast_io::fast_terminate(); }
            ::std::size_t total_extra_space{alignment + sizeofptr};
            ::std::size_t upperlimit{static_cast<::std::size_t>(mxn - total_extra_space)};
            if(n > upperlimit) { ::fast_io::fast_terminate(); }
            return n + total_extra_space;
        }

        inline void* allocator_adjust_ptr_to_aligned_impl(void* p, ::std::size_t alignment) noexcept
        {
            if(p == nullptr) [[unlikely]] { return nullptr; }
            void* aligned_ptr{reinterpret_cast<void*>((reinterpret_cast<::std::uintptr_t>(p) + alignment) & (0 - alignment))};
            reinterpret_cast<void**>(aligned_ptr)[-1] = p;
            return aligned_ptr;
        }

        template <typename>
        inline constexpr void* allocator_pointer_aligned_impl(::std::size_t, ::std::size_t, bool) noexcept;

        template <typename>
        inline constexpr ::fast_io::allocation_least_result allocator_pointer_aligned_at_least_impl(::std::size_t, ::std::size_t, bool) noexcept;

        template <typename alloc>
        inline constexpr void* status_allocator_pointer_aligned_impl(typename alloc::handle_type, ::std::size_t, ::std::size_t, bool) noexcept;

        template <typename alloc>
        inline constexpr ::fast_io::allocation_least_result
            status_allocator_pointer_aligned_at_least_impl(typename alloc::handle_type, ::std::size_t, ::std::size_t, bool) noexcept;

        inline constexpr ::fast_io::allocation_least_result make_allocation_least_result(void* ptr, ::std::size_t count) noexcept
        {
            if(ptr == nullptr) [[unlikely]] { return {nullptr, 0}; }
            return {ptr, count};
        }

        inline constexpr ::fast_io::allocation_least_result normalize_allocation_least_result(::fast_io::allocation_least_result res) noexcept
        { return make_allocation_least_result(res.ptr, res.count); }

        template <typename alloc>
        concept native_allocate_aligned_has_none_zero_ops = ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_impl<alloc> ||
                                                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_at_least_impl<alloc>;
        template <typename alloc>
        concept native_allocate_aligned_has_zero_ops = ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_impl<alloc> ||
                                                       ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_at_least_impl<alloc>;

        template <typename alloc>
        concept native_allocate_aligned_has_ops = ::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_aligned_has_none_zero_ops<alloc> ||
                                                  ::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_aligned_has_zero_ops<alloc>;

        template <typename alloc>
        concept native_allocate_has_none_zero_ops = ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_impl<alloc> ||
                                                    ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_at_least_impl<alloc> ||
                                                    ::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_aligned_has_none_zero_ops<alloc>;
        template <typename alloc>
        concept native_allocate_has_zero_ops = ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_impl<alloc> ||
                                               ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_at_least_impl<alloc> ||
                                               ::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_aligned_has_zero_ops<alloc>;
    }  // namespace details

#if 0
# include "allocator_adapter_flags.h"
#endif

    template <typename alloc>
    class fast_io_strict_generic_allocator_adapter
    {
    public:
        using allocator_type = alloc;
        using generic_allocator_adapter = fast_io_strict_generic_allocator_adapter<alloc>;
        using allocation_least_result = ::fast_io::allocation_least_result;
        inline static constexpr bool has_status{::uwvm2::utils::allocator::fast_io_strict::details::has_non_empty_handle_type<allocator_type>};

        template <typename T, bool = false>
        struct has
        {
            using type = allocator_type;  // any meaningless type other than void
        };

        template <typename T>
        struct has<T, true>
        {
            using type = typename T::handle_type;
        };

        using handle_type = typename has<allocator_type, has_status>::type;
        inline static constexpr ::std::size_t default_alignment{
            ::uwvm2::utils::allocator::fast_io_strict::details::calculate_default_alignment<allocator_type>()};
        inline static constexpr bool has_native_allocate{
            !has_status && (::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_at_least_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_at_least_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_at_least_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_at_least_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_conditional_zero_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_conditional_zero_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_conditional_zero_at_least_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_conditional_zero_at_least_impl<alloc>)};

        inline static void* allocate_conditional_zero(::std::size_t n, bool zero) noexcept
            requires (!has_status)
        {
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                auto p{
#if UWVM2_FAST_IO_STRICT_INTERNAL_HAS_BUILTIN(__builtin_operator_new)
                    __builtin_operator_new(n)
#else
                    ::operator new (n)
#endif
                };
                if(zero) { ::fast_io::freestanding::bytes_clear_n(static_cast<::std::byte*>(p), n); }
                return p;
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_conditional_zero_impl<alloc>)
                {
                    return allocator_type::allocate_conditional_zero(n, zero);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_conditional_zero_at_least_impl<alloc>)
                {
                    return allocator_type::allocate_conditional_zero_at_least(n, zero).ptr;
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_conditional_zero_impl<alloc>)
                {
                    return allocator_type::allocate_aligned_conditional_zero(default_alignment, n, zero);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_conditional_zero_at_least_impl<alloc>)
                {
                    return allocator_type::allocate_aligned_conditional_zero_at_least(default_alignment, n, zero).ptr;
                }
                else
                {
                    constexpr bool has_none_zero_ops{::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_has_none_zero_ops<alloc>};
                    constexpr bool has_zero_ops{::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_has_zero_ops<alloc>};
                    if constexpr(!has_none_zero_ops && !has_zero_ops)
                    {
                        ::fast_io::fast_terminate();
#if __has_cpp_attribute(unreachable)
                        [[unreachable]];
#endif
                    }
                    else if constexpr(!has_none_zero_ops && has_zero_ops)
                    {
                        if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_impl<alloc>)
                        {
                            return allocator_type::allocate_zero(n);
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_at_least_impl<alloc>)
                        {
                            return allocator_type::allocate_zero_at_least(n).ptr;
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_impl<alloc>)
                        {
                            return allocator_type::allocate_aligned_zero(default_alignment, n);
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_at_least_impl<alloc>)
                        {
                            return allocator_type::allocate_aligned_zero_at_least(default_alignment, n).ptr;
                        }
                    }
                    else if constexpr(has_none_zero_ops && !has_zero_ops)
                    {
                        void* ptr;
                        if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_impl<alloc>) { ptr = allocator_type::allocate(n); }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_at_least_impl<alloc>)
                        {
                            ptr = allocator_type::allocate_at_least(n).ptr;
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_impl<alloc>)
                        {
                            ptr = allocator_type::allocate_aligned(default_alignment, n);
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_at_least_impl<alloc>)
                        {
                            ptr = allocator_type::allocate_aligned_at_least(default_alignment, n).ptr;
                        }
                        if(zero && ptr != nullptr) { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(ptr), n); }
                        return ptr;
                    }
                    else
                    {
                        void* ptr;
                        if(zero)
                        {
                            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_impl<alloc>)
                            {
                                ptr = allocator_type::allocate_zero(n);
                            }
                            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_at_least_impl<alloc>)
                            {
                                ptr = allocator_type::allocate_zero_at_least(n).ptr;
                            }
                            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_impl<alloc>)
                            {
                                ptr = allocator_type::allocate_aligned_zero(default_alignment, n);
                            }
                            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_at_least_impl<alloc>)
                            {
                                ptr = allocator_type::allocate_aligned_zero_at_least(default_alignment, n).ptr;
                            }
                            else
                            {
                                ::fast_io::fast_terminate();
                            }
                        }
                        else
                        {
                            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_impl<alloc>) { ptr = allocator_type::allocate(n); }
                            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_at_least_impl<alloc>)
                            {
                                ptr = allocator_type::allocate_at_least(n).ptr;
                            }
                            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_impl<alloc>)
                            {
                                ptr = allocator_type::allocate_aligned(default_alignment, n);
                            }
                            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_at_least_impl<alloc>)
                            {
                                ptr = allocator_type::allocate_aligned_at_least(default_alignment, n).ptr;
                            }
                            else
                            {
                                ::fast_io::fast_terminate();
                            }
                        }
                        return ptr;
                    }
                }
            }
        }

        inline static constexpr void* allocate(::std::size_t n) noexcept
            requires (!has_status)
        {
#if __cpp_constexpr_dynamic_alloc >= 201907L
# if __cpp_if_consteval >= 202106L
            if consteval
# else
            if(__builtin_is_constant_evaluated())
# endif
            {
                return generic_allocator_adapter::allocate_conditional_zero(n, false);
            }
            else
#endif
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_impl<alloc>) { return allocator_type::allocate(n); }
                else
                {
                    return generic_allocator_adapter::allocate_conditional_zero(n, false);
                }
            }
        }

        inline static void* allocate_zero(::std::size_t n) noexcept
            requires (!has_status)
        {
#if __cpp_constexpr_dynamic_alloc >= 201907L
# if __cpp_if_consteval >= 202106L
            if consteval
# else
            if(__builtin_is_constant_evaluated())
# endif
            {
                return generic_allocator_adapter::allocate_conditional_zero(n, true);
            }
            else
#endif
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_impl<alloc>) { return allocator_type::allocate_zero(n); }
                else
                {
                    return generic_allocator_adapter::allocate_conditional_zero(n, true);
                }
            }
        }

        inline static constexpr bool has_reallocate =
            (::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_at_least_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_at_least_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_at_least_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_conditional_zero_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_conditional_zero_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_conditional_zero_at_least_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_conditional_zero_at_least_impl<alloc>);

        inline static void* reallocate_conditional_zero(void* p, ::std::size_t n, bool zero) noexcept
            requires (!has_status && has_reallocate)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_conditional_zero_impl<alloc>)
            {
                return allocator_type::reallocate_conditional_zero(p, n, zero);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_conditional_zero_at_least_impl<alloc>)
            {
                return allocator_type::reallocate_conditional_zero_at_least(p, n, zero).ptr;
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_conditional_zero_impl<alloc>)
            {
                return allocator_type::reallocate_aligned_conditional_zero(p, default_alignment, n, zero);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_conditional_zero_at_least_impl<alloc>)
            {
                return allocator_type::reallocate_aligned_conditional_zero_at_least(p, default_alignment, n, zero).ptr;
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_impl<alloc> ||
                              ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_at_least_impl<alloc> ||
                              ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_impl<alloc> ||
                              ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_at_least_impl<alloc>)
            {
                // Non-zero APIs exist - need runtime branch
                if(zero)
                {
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_impl<alloc>)
                    {
                        return allocator_type::reallocate_zero(p, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_zero_at_least(p, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_zero(p, default_alignment, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_zero_at_least(p, default_alignment, n).ptr;
                    }
                    else
                    {
                        ::fast_io::fast_terminate();
                    }
                }
                else
                {
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_impl<alloc>) { return allocator_type::reallocate(p, n); }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_at_least(p, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned(p, default_alignment, n);
                    }
                    else
                    {
                        return allocator_type::reallocate_aligned_at_least(p, default_alignment, n).ptr;
                    }
                }
            }
            else
            {
                // Only zero APIs exist - use zero API for both cases
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_impl<alloc>)
                {
                    return allocator_type::reallocate_zero(p, n);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_at_least_impl<alloc>)
                {
                    return allocator_type::reallocate_zero_at_least(p, n).ptr;
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc>)
                {
                    return allocator_type::reallocate_aligned_zero(p, default_alignment, n);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>)
                {
                    return allocator_type::reallocate_aligned_zero_at_least(p, default_alignment, n).ptr;
                }
                else
                {
                    ::fast_io::fast_terminate();
                }
            }
        }

        inline static void* reallocate(void* p, ::std::size_t n) noexcept
            requires (!has_status && has_reallocate)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_impl<alloc>) { return allocator_type::reallocate(p, n); }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_at_least_impl<alloc>)
            {
                return allocator_type::reallocate_at_least(p, n).ptr;
            }
            else
            {
                return generic_allocator_adapter::reallocate_conditional_zero(p, n, false);
            }
        }

        inline static constexpr bool has_reallocate_zero =
            (::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_at_least_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>);

        inline static void* reallocate_zero(void* p, ::std::size_t n) noexcept
            requires (!has_status && has_reallocate_zero)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_impl<alloc>) { return allocator_type::reallocate_zero(p, n); }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_at_least_impl<alloc>)
            {
                return allocator_type::reallocate_zero_at_least(p, n).ptr;
            }
            else
            {
                return generic_allocator_adapter::reallocate_conditional_zero(p, n, true);
            }
        }

        inline static void* reallocate_n_conditional_zero(void* p, ::std::size_t oldn, ::std::size_t n, bool zero) noexcept
            requires (!has_status)
        {
            if(p != nullptr && oldn == n) { return p; }
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                auto newptr{::operator new (n)};
                if(p != nullptr)
                {
                    if(n)
                    {
                        ::std::size_t copyn{oldn < n ? oldn : n};
                        ::fast_io::freestanding::nonoverlapped_bytes_copy_n(reinterpret_cast<::std::byte const*>(p),
                                                                            copyn,
                                                                            reinterpret_cast<::std::byte*>(newptr));
                    }
                    ::operator delete (p);
                }
                if(zero && oldn < n) { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(newptr) + oldn, n - oldn); }
                return newptr;
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_n_conditional_zero_impl<alloc>)
                {
                    return allocator_type::reallocate_n_conditional_zero(p, oldn, n, zero);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_n_conditional_zero_at_least_impl<alloc>)
                {
                    return allocator_type::reallocate_n_conditional_zero_at_least(p, oldn, n, zero).ptr;
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_conditional_zero_impl<alloc>)
                {
                    return allocator_type::reallocate_aligned_n_conditional_zero(p, oldn, default_alignment, n, zero);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_conditional_zero_at_least_impl<alloc>)
                {
                    return allocator_type::reallocate_aligned_n_conditional_zero_at_least(p, oldn, default_alignment, n, zero).ptr;
                }
                else if(zero)
                {
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_n_impl<alloc>)
                    {
                        return allocator_type::reallocate_zero_n(p, oldn, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_n_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_zero_n_at_least(p, oldn, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_zero_n(p, oldn, default_alignment, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_zero_n_at_least(p, oldn, default_alignment, n).ptr;
                    }
                    else
                    {
                        auto newptr{generic_allocator_adapter::reallocate_n(p, oldn, n)};
                        if(newptr == nullptr) [[unlikely]] { return nullptr; }
                        if(oldn < n) { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(newptr) + oldn, n - oldn); }
                        return newptr;
                    }
                }
                else
                {
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_n_impl<alloc>)
                    {
                        return allocator_type::reallocate_n(p, oldn, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_n_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_n_at_least(p, oldn, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_n(p, oldn, default_alignment, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_n_at_least(p, oldn, default_alignment, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_zero_n(p, oldn, default_alignment, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_zero_n_at_least(p, oldn, default_alignment, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_n_impl<alloc>)
                    {
                        return allocator_type::reallocate_zero_n(p, oldn, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_n_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_zero_n_at_least(p, oldn, n).ptr;
                    }
                    else
                    {
                        auto newptr{generic_allocator_adapter::allocate(n)};
                        if(newptr == nullptr) [[unlikely]] { return nullptr; }
                        if(p != nullptr)
                        {
                            if(n)
                            {
                                ::std::size_t copyn{oldn < n ? oldn : n};
                                ::fast_io::freestanding::nonoverlapped_bytes_copy_n(reinterpret_cast<::std::byte const*>(p),
                                                                                    copyn,
                                                                                    reinterpret_cast<::std::byte*>(newptr));
                            }
                            generic_allocator_adapter::deallocate_n(p, oldn);
                        }
                        return newptr;
                    }
                }
            }
        }

        inline static void* reallocate_n(void* p, ::std::size_t oldn, ::std::size_t n) noexcept
            requires (!has_status)
        {
            if(p != nullptr && oldn == n) { return p; }
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_n_impl<alloc>) { return allocator_type::reallocate_n(p, oldn, n); }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_n_at_least_impl<alloc>)
            {
                return allocator_type::reallocate_n_at_least(p, oldn, n).ptr;
            }
            else
            {
                return generic_allocator_adapter::reallocate_n_conditional_zero(p, oldn, n, false);
            }
        }

        inline static void* reallocate_zero_n(void* p, ::std::size_t oldn, ::std::size_t n) noexcept
            requires (!has_status)
        {
            if(p != nullptr && oldn == n) { return p; }
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_n_impl<alloc>)
            {
                return allocator_type::reallocate_zero_n(p, oldn, n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_n_at_least_impl<alloc>)
            {
                return allocator_type::reallocate_zero_n_at_least(p, oldn, n).ptr;
            }
            else
            {
                return generic_allocator_adapter::reallocate_n_conditional_zero(p, oldn, n, true);
            }
        }

        inline static constexpr bool has_deallocate = (::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_impl<alloc> ||
                                                       ::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_aligned_impl<alloc>);

        inline static constexpr void deallocate(void* p) noexcept
            requires (!has_status && has_deallocate)
        {
#if __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
            {
# if UWVM2_FAST_IO_STRICT_INTERNAL_HAS_BUILTIN(__builtin_operator_delete)
                __builtin_operator_delete(p);
# else
                ::operator delete (p);
# endif
            }
            else
#endif
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_impl<alloc>) { allocator_type::deallocate(p); }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_aligned_impl<alloc>)
                {
                    allocator_type::deallocate_aligned(p, default_alignment);
                }
            }
        }

        inline static constexpr void deallocate_n(void* p, ::std::size_t n) noexcept
            requires (!has_status)
        {
#if __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
            {
# if UWVM2_FAST_IO_STRICT_INTERNAL_HAS_BUILTIN(__builtin_operator_delete)
                __builtin_operator_delete(p);
# else
                ::operator delete (p);
# endif
            }
            else
#endif
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_n_impl<alloc>) { allocator_type::deallocate_n(p, n); }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_aligned_n_impl<alloc>)
                {
                    allocator_type::deallocate_aligned_n(p, default_alignment, n);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_impl<alloc>) { allocator_type::deallocate(p); }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_aligned_impl<alloc>)
                {
                    allocator_type::deallocate_aligned(p, default_alignment);
                }
                else
                {
                    static_assert(::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_n_impl<alloc>);
                }
            }
        }

        inline static constexpr void* allocate_aligned_conditional_zero(::std::size_t alignment, ::std::size_t n, bool zero) noexcept
            requires (!has_status)
        {
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                auto p{
#if UWVM2_FAST_IO_STRICT_INTERNAL_HAS_BUILTIN(__builtin_operator_new)
                    __builtin_operator_new(n)
#else
                    ::operator new (n)
#endif
                };
                if(zero) { ::fast_io::freestanding::bytes_clear_n(static_cast<::std::byte*>(p), n); }
                return p;
            }
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_conditional_zero_impl<alloc>)
            {
                return allocator_type::allocate_aligned_conditional_zero(alignment, n, zero);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_conditional_zero_at_least_impl<alloc>)
            {
                return allocator_type::allocate_aligned_conditional_zero_at_least(alignment, n, zero).ptr;
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_aligned_has_ops<alloc>)
            {
                constexpr bool has_none_zero_ops{::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_aligned_has_none_zero_ops<alloc>};
                constexpr bool has_zero_ops{::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_aligned_has_zero_ops<alloc>};
                if constexpr(!has_none_zero_ops && has_zero_ops)
                {
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_impl<alloc>)
                    {
                        return allocator_type::allocate_aligned_zero(alignment, n);
                    }
                    else
                    {
                        return allocator_type::allocate_aligned_zero_at_least(alignment, n).ptr;
                    }
                }
                else if constexpr(has_none_zero_ops && !has_zero_ops)
                {
                    void* ptr;
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_impl<alloc>)
                    {
                        ptr = allocator_type::allocate_aligned(alignment, n);
                    }
                    else
                    {
                        ptr = allocator_type::allocate_aligned_at_least(alignment, n).ptr;
                    }
                    if(zero && ptr != nullptr) { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(ptr), n); }
                    return ptr;
                }
                else if constexpr(has_none_zero_ops && has_zero_ops)
                {
                    void* ptr;
                    if(zero)
                    {
                        if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_impl<alloc>)
                        {
                            ptr = allocator_type::allocate_aligned_zero(alignment, n);
                        }
                        else
                        {
                            ptr = allocator_type::allocate_aligned_zero_at_least(alignment, n).ptr;
                        }
                    }
                    else
                    {
                        if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_impl<alloc>)
                        {
                            ptr = allocator_type::allocate_aligned(alignment, n);
                        }
                        else
                        {
                            ptr = allocator_type::allocate_aligned_at_least(alignment, n).ptr;
                        }
                    }
                    return ptr;
                }
                else
                {
                    ::fast_io::fast_terminate();
                }
            }
            else
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::allocator_pointer_aligned_impl<alloc>(alignment, n, zero);
            }
        }

        inline static constexpr void* allocate_aligned(::std::size_t alignment, ::std::size_t n) noexcept
            requires (!has_status)
        {
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                return
#if UWVM2_FAST_IO_STRICT_INTERNAL_HAS_BUILTIN(__builtin_operator_new)
                    __builtin_operator_new(n)
#else
                    ::operator new (n)
#endif
                        ;
            }
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_impl<alloc>)
            {
                return allocator_type::allocate_aligned(alignment, n);
            }
            else
            {
                return generic_allocator_adapter::allocate_aligned_conditional_zero(alignment, n, false);
            }
        }

        inline static constexpr void* allocate_aligned_zero(::std::size_t alignment, ::std::size_t n) noexcept
            requires (!has_status)
        {
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                return
#if UWVM2_FAST_IO_STRICT_INTERNAL_HAS_BUILTIN(__builtin_operator_new)
                    __builtin_operator_new(n)
#else
                    ::operator new (n)
#endif
                        ;
            }
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_impl<alloc>)
            {
                return allocator_type::allocate_aligned_zero(alignment, n);
            }
            else
            {
                return generic_allocator_adapter::allocate_aligned_conditional_zero(alignment, n, true);
            }
        }

        inline static constexpr bool has_native_allocate_at_least{
            !has_status && (::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_at_least_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_at_least_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_at_least_impl<alloc> ||
                            ::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_at_least_impl<alloc>)};

        inline static constexpr allocation_least_result allocate_at_least(::std::size_t n) noexcept
            requires (!has_status)
        {
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                return {
#if UWVM2_FAST_IO_STRICT_INTERNAL_HAS_BUILTIN(__builtin_operator_new)
                    __builtin_operator_new(n)
#else
                    ::operator new (n)
#endif
                        ,
                    n};
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_at_least_impl<alloc>)
                {
                    return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(allocator_type::allocate_at_least(n));
                }
                else
                {
                    return generic_allocator_adapter::allocate_conditional_zero_at_least(n, false);
                }
            }
        }

        inline static constexpr allocation_least_result allocate_zero_at_least(::std::size_t n) noexcept
            requires (!has_status)
        {
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                return {
#if UWVM2_FAST_IO_STRICT_INTERNAL_HAS_BUILTIN(__builtin_operator_new)
                    __builtin_operator_new(n)
#else
                    ::operator new (n)
#endif
                        ,
                    n};
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_at_least_impl<alloc>)
                {
                    return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(allocator_type::allocate_zero_at_least(n));
                }
                else
                {
                    return generic_allocator_adapter::allocate_conditional_zero_at_least(n, true);
                }
            }
        }

        inline static constexpr allocation_least_result allocate_conditional_zero_at_least(::std::size_t n, bool zero) noexcept
            requires (!has_status)
        {
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                auto p{::operator new (n)};
                if(zero) { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(p), n); }
                return {p, n};
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_conditional_zero_at_least_impl<alloc>)
                {
                    return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                        allocator_type::allocate_conditional_zero_at_least(n, zero));
                }
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_conditional_zero_impl<alloc>)
                {
                    return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::allocate_conditional_zero(n, zero),
                                                                                                            n);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_conditional_zero_at_least_impl<alloc>)
                {
                    return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                        allocator_type::allocate_aligned_conditional_zero_at_least(default_alignment, n, zero));
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_conditional_zero_impl<alloc>)
                {
                    return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                        allocator_type::allocate_aligned_conditional_zero(default_alignment, n, zero),
                        n);
                }
                else
                {
                    constexpr bool has_none_zero_ops{::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_has_none_zero_ops<alloc>};
                    constexpr bool has_zero_ops{::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_has_zero_ops<alloc>};
                    if constexpr(!has_none_zero_ops && !has_zero_ops)
                    {
                        ::fast_io::fast_terminate();
#if __has_cpp_attribute(unreachable)
                        [[unreachable]];
#endif
                    }
                    else if constexpr(!has_none_zero_ops && has_zero_ops)
                    {
                        if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_at_least_impl<alloc>)
                        {
                            return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                                allocator_type::allocate_zero_at_least(n));
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_impl<alloc>)
                        {
                            return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::allocate_zero(n), n);
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_at_least_impl<alloc>)
                        {
                            return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                                allocator_type::allocate_aligned_zero_at_least(default_alignment, n));
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_impl<alloc>)
                        {
                            return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                                allocator_type::allocate_aligned_zero(default_alignment, n),
                                n);
                        }
                    }
                    else if constexpr(has_none_zero_ops && !has_zero_ops)
                    {
                        ::fast_io::allocation_least_result res;
                        if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_at_least_impl<alloc>)
                        {
                            res = allocator_type::allocate_at_least(n);
                            res = ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(res);
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_impl<alloc>)
                        {
                            res = ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::allocate(n), n);
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_at_least_impl<alloc>)
                        {
                            res = allocator_type::allocate_aligned_at_least(default_alignment, n);
                            res = ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(res);
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_impl<alloc>)
                        {
                            res = ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                                allocator_type::allocate_aligned(default_alignment, n),
                                n);
                        }
                        if(zero && res.ptr != nullptr) { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(res.ptr), res.count); }
                        return res;
                    }
                    else
                    {
                        ::fast_io::allocation_least_result res;
                        if(zero)
                        {
                            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_at_least_impl<alloc>)
                            {
                                res = allocator_type::allocate_zero_at_least(n);
                                res = ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(res);
                            }
                            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_zero_impl<alloc>)
                            {
                                res = ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::allocate_zero(n), n);
                            }
                            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_at_least_impl<alloc>)
                            {
                                res = allocator_type::allocate_aligned_zero_at_least(default_alignment, n);
                                res = ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(res);
                            }
                            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_impl<alloc>)
                            {
                                res = ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                                    allocator_type::allocate_aligned_zero(default_alignment, n),
                                    n);
                            }
                            else
                            {
                                ::fast_io::fast_terminate();
                            }
                        }
                        else
                        {
                            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_at_least_impl<alloc>)
                            {
                                res = allocator_type::allocate_at_least(n);
                                res = ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(res);
                            }
                            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_impl<alloc>)
                            {
                                res = ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::allocate(n), n);
                            }
                            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_at_least_impl<alloc>)
                            {
                                res = allocator_type::allocate_aligned_at_least(default_alignment, n);
                                res = ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(res);
                            }
                            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_impl<alloc>)
                            {
                                res = ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                                    allocator_type::allocate_aligned(default_alignment, n),
                                    n);
                            }
                            else
                            {
                                ::fast_io::fast_terminate();
                            }
                        }
                        return res;
                    }
                }
            }
        }

        inline static constexpr allocation_least_result allocate_aligned_at_least(::std::size_t alignment, ::std::size_t n) noexcept
            requires (!has_status)
        {
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                return generic_allocator_adapter::allocate_aligned_conditional_zero_at_least(alignment, n, false);
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_at_least_impl<alloc>)
                {
                    return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                        allocator_type::allocate_aligned_at_least(alignment, n));
                }
                else
                {
                    return generic_allocator_adapter::allocate_aligned_conditional_zero_at_least(alignment, n, false);
                }
            }
        }

        inline static constexpr allocation_least_result allocate_aligned_zero_at_least(::std::size_t alignment, ::std::size_t n) noexcept
            requires (!has_status)
        {
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                return generic_allocator_adapter::allocate_aligned_conditional_zero_at_least(alignment, n, true);
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_at_least_impl<alloc>)
                {
                    return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                        allocator_type::allocate_aligned_zero_at_least(alignment, n));
                }
                else
                {
                    return generic_allocator_adapter::allocate_aligned_conditional_zero_at_least(alignment, n, true);
                }
            }
        }

        inline static constexpr allocation_least_result allocate_aligned_conditional_zero_at_least(::std::size_t alignment, ::std::size_t n, bool zero) noexcept
            requires (!has_status)
        {
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                auto p{::operator new (n)};
                if(zero) { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(p), n); }
                return {p, n};
            }
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_conditional_zero_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::allocate_aligned_conditional_zero_at_least(alignment, n, zero));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_conditional_zero_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::allocate_aligned_conditional_zero(alignment, n, zero),
                    n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_aligned_has_ops<alloc>)
            {
                constexpr bool has_none_zero_ops{::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_aligned_has_none_zero_ops<alloc>};
                constexpr bool has_zero_ops{::uwvm2::utils::allocator::fast_io_strict::details::native_allocate_aligned_has_zero_ops<alloc>};
                if constexpr(!has_none_zero_ops && has_zero_ops)
                {
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_at_least_impl<alloc>)
                    {
                        return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                            allocator_type::allocate_aligned_zero_at_least(alignment, n));
                    }
                    else
                    {
                        return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                            allocator_type::allocate_aligned_zero(alignment, n),
                            n);
                    }
                }
                else if constexpr(has_none_zero_ops && !has_zero_ops)
                {
                    ::fast_io::allocation_least_result res;
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_at_least_impl<alloc>)
                    {
                        res = allocator_type::allocate_aligned_at_least(alignment, n);
                        res = ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(res);
                    }
                    else
                    {
                        res =
                            ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::allocate_aligned(alignment, n), n);
                    }
                    if(zero && res.ptr != nullptr) { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(res.ptr), res.count); }
                    return res;
                }
                else if constexpr(has_none_zero_ops && has_zero_ops)
                {
                    ::fast_io::allocation_least_result res;
                    if(zero)
                    {
                        if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_zero_at_least_impl<alloc>)
                        {
                            res = allocator_type::allocate_aligned_zero_at_least(alignment, n);
                            res = ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(res);
                        }
                        else
                        {
                            res = ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                                allocator_type::allocate_aligned_zero(alignment, n),
                                n);
                        }
                    }
                    else
                    {
                        if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_allocate_aligned_at_least_impl<alloc>)
                        {
                            res = allocator_type::allocate_aligned_at_least(alignment, n);
                            res = ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(res);
                        }
                        else
                        {
                            res =
                                ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::allocate_aligned(alignment, n),
                                                                                                                 n);
                        }
                    }
                    return res;
                }
                else
                {
                    ::fast_io::fast_terminate();
                }
            }
            else
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::allocator_pointer_aligned_at_least_impl<alloc>(alignment, n, zero);
            }
        }

        inline static constexpr bool has_reallocate_aligned =
            (::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_at_least_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_conditional_zero_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_conditional_zero_at_least_impl<alloc>);

        inline static void* reallocate_aligned_conditional_zero(void* p, ::std::size_t alignment, ::std::size_t n, bool zero) noexcept
            requires (!has_status && has_reallocate_aligned)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_conditional_zero_impl<alloc>)
            {
                return allocator_type::reallocate_aligned_conditional_zero(p, alignment, n, zero);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_conditional_zero_at_least_impl<alloc>)
            {
                return allocator_type::reallocate_aligned_conditional_zero_at_least(p, alignment, n, zero).ptr;
            }
            else if(zero)
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc>)
                {
                    return allocator_type::reallocate_aligned_zero(p, alignment, n);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>)
                {
                    return allocator_type::reallocate_aligned_zero_at_least(p, alignment, n).ptr;
                }
                else
                {
                    ::fast_io::fast_terminate();
                }
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_impl<alloc>)
                {
                    return allocator_type::reallocate_aligned(p, alignment, n);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_at_least_impl<alloc>)
                {
                    return allocator_type::reallocate_aligned_at_least(p, alignment, n).ptr;
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc>)
                {
                    return allocator_type::reallocate_aligned_zero(p, alignment, n);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>)
                {
                    return allocator_type::reallocate_aligned_zero_at_least(p, alignment, n).ptr;
                }
                else
                {
                    ::fast_io::fast_terminate();
                }
            }
        }

        inline static void* reallocate_aligned(void* p, ::std::size_t alignment, ::std::size_t n) noexcept
            requires (!has_status && has_reallocate_aligned)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_impl<alloc>)
            {
                return allocator_type::reallocate_aligned(p, alignment, n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_at_least_impl<alloc>)
            {
                return allocator_type::reallocate_aligned_at_least(p, alignment, n).ptr;
            }
            else
            {
                return generic_allocator_adapter::reallocate_aligned_conditional_zero(p, alignment, n, false);
            }
        }

        inline static constexpr bool has_reallocate_aligned_zero =
            (::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>);

        inline static void* reallocate_aligned_zero(void* p, ::std::size_t alignment, ::std::size_t n) noexcept
            requires (!has_status && has_reallocate_aligned_zero)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc>)
            {
                return allocator_type::reallocate_aligned_zero(p, alignment, n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>)
            {
                return allocator_type::reallocate_aligned_zero_at_least(p, alignment, n).ptr;
            }
            else
            {
                return generic_allocator_adapter::reallocate_aligned_conditional_zero(p, alignment, n, true);
            }
        }

        inline static void* reallocate_aligned_n_conditional_zero(void* p, ::std::size_t oldn, ::std::size_t alignment, ::std::size_t n, bool zero) noexcept
            requires (!has_status)
        {
            if(p != nullptr && oldn == n) { return p; }
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                auto newptr{::operator new (n)};
                if(p != nullptr)
                {
                    if(n)
                    {
                        ::std::size_t copyn{oldn < n ? oldn : n};
                        ::fast_io::freestanding::nonoverlapped_bytes_copy_n(reinterpret_cast<::std::byte const*>(p),
                                                                            copyn,
                                                                            reinterpret_cast<::std::byte*>(newptr));
                    }
                    ::operator delete (p);
                }
                if(zero && oldn < n) { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(newptr) + oldn, n - oldn); }
                return newptr;
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_conditional_zero_impl<alloc>)
                {
                    return allocator_type::reallocate_aligned_n_conditional_zero(p, oldn, alignment, n, zero);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_conditional_zero_at_least_impl<alloc>)
                {
                    return allocator_type::reallocate_aligned_n_conditional_zero_at_least(p, oldn, alignment, n, zero).ptr;
                }
                else if(zero)
                {
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_zero_n(p, oldn, alignment, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_zero_n_at_least(p, oldn, alignment, n).ptr;
                    }
                    else
                    {
                        auto newptr{generic_allocator_adapter::reallocate_aligned_n(p, oldn, alignment, n)};
                        if(newptr == nullptr) [[unlikely]] { return nullptr; }
                        if(oldn < n) { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(newptr) + oldn, n - oldn); }
                        return newptr;
                    }
                }
                else
                {
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_n(p, oldn, alignment, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_n_at_least(p, oldn, alignment, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned(p, alignment, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_at_least(p, alignment, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_zero_n(p, oldn, alignment, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_zero_n_at_least(p, oldn, alignment, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_zero(p, alignment, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>)
                    {
                        return allocator_type::reallocate_aligned_zero_at_least(p, alignment, n).ptr;
                    }
                    else
                    {
                        if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_n_impl<alloc> ||
                                     ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_n_impl<alloc> ||
                                     ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_impl<alloc> ||
                                     ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_impl<alloc>)
                        {
                            if(alignment <= default_alignment) { return generic_allocator_adapter::reallocate_n(p, oldn, n); }
                        }
                        auto newptr{::uwvm2::utils::allocator::fast_io_strict::details::allocator_pointer_aligned_impl<alloc>(alignment, n, false)};
                        if(newptr == nullptr) [[unlikely]] { return nullptr; }
                        if(p != nullptr)
                        {
                            if(n)
                            {
                                bool moren{oldn < n};
                                ::std::size_t copyn{moren ? oldn : n};
                                ::fast_io::freestanding::nonoverlapped_bytes_copy_n(reinterpret_cast<::std::byte const*>(p),
                                                                                    copyn,
                                                                                    reinterpret_cast<::std::byte*>(newptr));
                            }
                            generic_allocator_adapter::deallocate_aligned_n(p, alignment, oldn);
                        }
                        return newptr;
                    }
                }
            }
        }

        inline static void* reallocate_aligned_n(void* p, ::std::size_t oldn, ::std::size_t alignment, ::std::size_t n) noexcept
            requires (!has_status)
        {
            if(p != nullptr && oldn == n) { return p; }
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_impl<alloc>)
            {
                return allocator_type::reallocate_aligned_n(p, oldn, alignment, n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_at_least_impl<alloc>)
            {
                return allocator_type::reallocate_aligned_n_at_least(p, oldn, alignment, n).ptr;
            }
            else
            {
                return generic_allocator_adapter::reallocate_aligned_n_conditional_zero(p, oldn, alignment, n, false);
            }
        }

        inline static void* reallocate_aligned_zero_n(void* p, ::std::size_t oldn, ::std::size_t alignment, ::std::size_t n) noexcept
            requires (!has_status)
        {
            if(p != nullptr && oldn == n) { return p; }
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_impl<alloc>)
            {
                return allocator_type::reallocate_aligned_zero_n(p, oldn, alignment, n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_at_least_impl<alloc>)
            {
                return allocator_type::reallocate_aligned_zero_n_at_least(p, oldn, alignment, n).ptr;
            }
            else
            {
                auto newptr{generic_allocator_adapter::reallocate_aligned_n(p, oldn, alignment, n)};
                if(newptr == nullptr) [[unlikely]] { return nullptr; }
                if(oldn < n)
                {
                    ::std::size_t const to_zero_bytes{static_cast<::std::size_t>(n - oldn)};
                    ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(newptr) + oldn, to_zero_bytes);
                }
                return newptr;
            }
        }

        inline static constexpr bool has_native_reallocate_at_least =
            (has_reallocate && (::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_at_least_impl<alloc> ||
                                ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_at_least_impl<alloc> ||
                                ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_at_least_impl<alloc> ||
                                ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>));

        inline static ::fast_io::allocation_least_result reallocate_at_least(void* p, ::std::size_t n) noexcept
            requires (!has_status && has_reallocate)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(allocator_type::reallocate_at_least(p, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_at_least(p, default_alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(allocator_type::reallocate_zero_at_least(p, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_at_least(p, default_alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::reallocate(p, n), n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::reallocate_aligned(p, default_alignment, n),
                    n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::reallocate_zero(p, n), n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::reallocate_aligned_zero(p, default_alignment, n),
                    n);
            }
        }

        inline static constexpr bool has_native_reallocate_zero_at_least =
            (has_reallocate_zero && (::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_at_least_impl<alloc> ||
                                     ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>));

        inline static ::fast_io::allocation_least_result reallocate_zero_at_least(void* p, ::std::size_t n) noexcept
            requires (!has_status && has_reallocate_zero)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(allocator_type::reallocate_zero_at_least(p, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_at_least(p, default_alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::reallocate_zero(p, n), n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::reallocate_aligned_zero(p, default_alignment, n),
                    n);
            }
        }

        inline static ::fast_io::allocation_least_result reallocate_n_at_least(void* p, ::std::size_t oldn, ::std::size_t n) noexcept
            requires (!has_status)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_n_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(allocator_type::reallocate_n_at_least(p, oldn, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_n_at_least(p, oldn, default_alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_n_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_zero_n_at_least(p, oldn, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_n_at_least(p, oldn, default_alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(allocator_type::reallocate_at_least(p, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_at_least(p, default_alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(allocator_type::reallocate_zero_at_least(p, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_at_least(p, default_alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_n_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::reallocate_n(p, oldn, n), n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::reallocate_aligned_n(p, oldn, default_alignment, n),
                    n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_n_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::reallocate_zero_n(p, oldn, n), n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_n(p, oldn, default_alignment, n),
                    n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::reallocate(p, n), n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::reallocate_aligned(p, default_alignment, n),
                    n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::reallocate_zero(p, n), n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::reallocate_aligned_zero(p, default_alignment, n),
                    n);
            }
            else
            {
                auto newres{generic_allocator_adapter::allocate_at_least(n)};
                auto newptr{newres.ptr};
                if(newptr == nullptr) [[unlikely]] { return {nullptr, 0}; }
                if(p != nullptr)
                {
                    if(n)
                    {
                        if(oldn < n) { n = oldn; }
                        ::fast_io::freestanding::nonoverlapped_bytes_copy_n(reinterpret_cast<::std::byte const*>(p), n, reinterpret_cast<::std::byte*>(newptr));
                    }
                    generic_allocator_adapter::deallocate_n(p, oldn);
                }
                return newres;
            }
        }

        inline static ::fast_io::allocation_least_result reallocate_zero_n_at_least(void* p, ::std::size_t oldn, ::std::size_t n) noexcept
            requires (!has_status)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_n_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_zero_n_at_least(p, oldn, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_n_at_least(p, oldn, default_alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(allocator_type::reallocate_zero_at_least(p, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_at_least(p, default_alignment, n));
            }
            else
            {
                auto newres{generic_allocator_adapter::reallocate_n_at_least(p, oldn, n)};
                auto newptr{newres.ptr};
                if(newptr == nullptr) [[unlikely]] { return {nullptr, 0}; }
                n = newres.count;
                if(oldn < n)
                {
                    ::std::size_t const to_zero_bytes{static_cast<::std::size_t>(n - oldn)};
                    ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(newptr) + oldn, to_zero_bytes);
                }
                return newres;
            }
        }

        inline static constexpr bool has_native_reallocate_aligned_at_least =
            (has_reallocate_aligned && (::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc> ||
                                        ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_at_least_impl<alloc>));

        inline static ::fast_io::allocation_least_result reallocate_aligned_at_least(void* p, ::std::size_t alignment, ::std::size_t n) noexcept
            requires (!has_status && has_reallocate_aligned)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_at_least(p, alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_at_least(p, alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::reallocate_aligned(p, alignment, n), n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::reallocate_aligned_zero(p, alignment, n),
                    n);
            }
        }

        inline static constexpr bool has_native_reallocate_aligned_zero_at_least =
            (has_reallocate_aligned_zero && ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>);

        inline static ::fast_io::allocation_least_result reallocate_aligned_zero_at_least(void* p, ::std::size_t alignment, ::std::size_t n) noexcept
            requires (!has_status && has_reallocate_aligned_zero)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_at_least(p, alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::reallocate_aligned_zero(p, alignment, n),
                    n);
            }
        }

        inline static constexpr bool has_native_reallocate_aligned_n_at_least =
            (::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_at_least_impl<alloc> ||
             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_at_least_impl<alloc>);

        inline static ::fast_io::allocation_least_result
            reallocate_aligned_n_at_least(void* p, ::std::size_t oldn, ::std::size_t alignment, ::std::size_t n) noexcept
            requires (!has_status)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_n_at_least(p, oldn, alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_at_least(p, alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_n_at_least(p, oldn, alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_at_least(p, alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_n_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::reallocate_aligned_n(p, oldn, alignment, n),
                    n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(allocator_type::reallocate_aligned(p, alignment, n), n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_n(p, oldn, alignment, n),
                    n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::reallocate_aligned_zero(p, alignment, n),
                    n);
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_at_least_impl<alloc> ||
                             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_at_least_impl<alloc> ||
                             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_n_at_least_impl<alloc> ||
                             ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_zero_n_at_least_impl<alloc>)
                {
                    if(alignment <= default_alignment) { return generic_allocator_adapter::reallocate_n_at_least(p, oldn, n); }
                }
                auto newres{::uwvm2::utils::allocator::fast_io_strict::details::allocator_pointer_aligned_at_least_impl<alloc>(alignment, n, false)};
                auto newptr{newres.ptr};
                if(newptr == nullptr) [[unlikely]] { return {nullptr, 0}; }
                if(p != nullptr)
                {
                    if(n)
                    {
                        if(oldn < n) { n = oldn; }
                        ::fast_io::freestanding::nonoverlapped_bytes_copy_n(reinterpret_cast<::std::byte const*>(p), n, reinterpret_cast<::std::byte*>(newptr));
                    }
                    generic_allocator_adapter::deallocate_aligned_n(p, alignment, oldn);
                }
                return newres;
            }
        }

        inline static constexpr bool has_native_reallocate_aligned_zero_n_at_least =
            ::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_at_least_impl<alloc>;

        inline static ::fast_io::allocation_least_result
            reallocate_aligned_zero_n_at_least(void* p, ::std::size_t oldn, ::std::size_t alignment, ::std::size_t n) noexcept
            requires (!has_status)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_at_least_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::normalize_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_n_at_least(p, oldn, alignment, n));
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_reallocate_aligned_zero_n_impl<alloc>)
            {
                return ::uwvm2::utils::allocator::fast_io_strict::details::make_allocation_least_result(
                    allocator_type::reallocate_aligned_zero_n(p, oldn, alignment, n),
                    n);
            }
            else
            {
                auto newres = generic_allocator_adapter::reallocate_aligned_n_at_least(p, oldn, alignment, n);
                auto newptr{newres.ptr};
                if(newptr == nullptr) [[unlikely]] { return {nullptr, 0}; }
                n = newres.count;
                if(oldn < n)
                {
                    ::std::size_t const to_zero_bytes{static_cast<::std::size_t>(n - oldn)};
                    ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(newptr) + oldn, to_zero_bytes);
                }
                return newres;
            }
        }

        inline static constexpr bool has_deallocate_aligned = (::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_aligned_impl<alloc> ||
                                                               ::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_impl<alloc>);

        inline static void deallocate_aligned(void* p, ::std::size_t alignment) noexcept
            requires (!has_status && has_deallocate_aligned)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_aligned_impl<alloc>)
            {
                allocator_type::deallocate_aligned(p, alignment);
            }
            else
            {
                if(p == nullptr) { return; }
                if(default_alignment < alignment) { p = reinterpret_cast<void**>(p)[-1]; }
                allocator_type::deallocate(p);
            }
        }

        inline static void deallocate_aligned_n(void* p, ::std::size_t alignment, ::std::size_t n) noexcept
            requires (!has_status)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_aligned_n_impl<alloc>)
            {
                allocator_type::deallocate_aligned_n(p, alignment, n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_aligned_impl<alloc>)
            {
                allocator_type::deallocate_aligned(p, alignment);
            }
            else
            {
                if(p == nullptr) { return; }
                if(default_alignment < alignment)
                {
                    auto start{reinterpret_cast<void**>(p)[-1]};
                    n += static_cast<::std::size_t>(reinterpret_cast<char unsigned*>(p) - reinterpret_cast<char unsigned*>(start));
                    p = start;
                }
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_deallocate_impl<alloc>) { allocator_type::deallocate(p); }
                else
                {
                    allocator_type::deallocate_n(p, n);
                }
            }
        }

        // Handle-based allocation functions (for allocators with non-empty handle_type)
        inline static constexpr bool has_native_handle_allocate{
            has_status && (::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_zero_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_zero_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_zero_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_zero_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_conditional_zero_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_conditional_zero_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_conditional_zero_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_conditional_zero_at_least_impl<alloc>)};

        inline static void* handle_allocate_conditional_zero(handle_type handle, ::std::size_t n, bool zero) noexcept
            requires (has_status && has_native_handle_allocate)
        {
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                auto p{::operator new (n)};
                if(zero) { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(p), n); }
                return p;
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_conditional_zero_impl<alloc>)
                {
                    return allocator_type::handle_allocate_conditional_zero(handle, n, zero);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_conditional_zero_at_least_impl<alloc>)
                {
                    return allocator_type::handle_allocate_conditional_zero_at_least(handle, n, zero).ptr;
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_conditional_zero_impl<alloc>)
                {
                    return allocator_type::handle_allocate_aligned_conditional_zero(handle, default_alignment, n, zero);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_conditional_zero_at_least_impl<alloc>)
                {
                    return allocator_type::handle_allocate_aligned_conditional_zero_at_least(handle, default_alignment, n, zero).ptr;
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_impl<alloc> ||
                                  ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_at_least_impl<alloc> ||
                                  ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_impl<alloc> ||
                                  ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_at_least_impl<alloc>)
                {
                    if(zero)
                    {
                        if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_zero_impl<alloc>)
                        {
                            return allocator_type::handle_allocate_zero(handle, n);
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_zero_at_least_impl<alloc>)
                        {
                            return allocator_type::handle_allocate_zero_at_least(handle, n).ptr;
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_zero_impl<alloc>)
                        {
                            return allocator_type::handle_allocate_aligned_zero(handle, default_alignment, n);
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_zero_at_least_impl<alloc>)
                        {
                            return allocator_type::handle_allocate_aligned_zero_at_least(handle, default_alignment, n).ptr;
                        }
                        else
                        {
                            auto p{generic_allocator_adapter::handle_allocate(handle, n)};
                            if(p != nullptr) [[likely]] { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(p), n); }
                            return p;
                        }
                    }
                    else
                    {
                        if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_impl<alloc>)
                        {
                            return allocator_type::handle_allocate(handle, n);
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_at_least_impl<alloc>)
                        {
                            return allocator_type::handle_allocate_at_least(handle, n).ptr;
                        }
                        else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_impl<alloc>)
                        {
                            return allocator_type::handle_allocate_aligned(handle, default_alignment, n);
                        }
                        else
                        {
                            return allocator_type::handle_allocate_aligned_at_least(handle, default_alignment, n).ptr;
                        }
                    }
                }
                else
                {
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_zero_impl<alloc>)
                    {
                        return allocator_type::handle_allocate_zero(handle, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_zero_at_least_impl<alloc>)
                    {
                        return allocator_type::handle_allocate_zero_at_least(handle, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_zero_impl<alloc>)
                    {
                        return allocator_type::handle_allocate_aligned_zero(handle, default_alignment, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_aligned_zero_at_least_impl<alloc>)
                    {
                        return allocator_type::handle_allocate_aligned_zero_at_least(handle, default_alignment, n).ptr;
                    }
                    else
                    {
                        ::fast_io::fast_terminate();
                    }
                }
            }
        }

        inline static void* handle_allocate(handle_type handle, ::std::size_t n) noexcept
            requires (has_status && has_native_handle_allocate)
        {
#if __cpp_if_consteval >= 202106L
            if consteval
#elif __cpp_lib_is_constant_evaluated >= 201811L && __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
#else
            if(false)
#endif
            {
                return static_cast<void*>(::fast_io::freestanding::allocator<::std::byte>{}.allocate(n));
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_impl<alloc>)
                {
                    return allocator_type::handle_allocate(handle, n);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_at_least_impl<alloc>)
                {
                    return allocator_type::handle_allocate_at_least(handle, n).ptr;
                }
                else
                {
                    return generic_allocator_adapter::handle_allocate_conditional_zero(handle, n, false);
                }
            }
        }

        inline static void* handle_allocate_zero(handle_type handle, ::std::size_t n) noexcept
            requires (has_status && has_native_handle_allocate)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_zero_impl<alloc>)
            {
                return allocator_type::handle_allocate_zero(handle, n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_allocate_zero_at_least_impl<alloc>)
            {
                return allocator_type::handle_allocate_zero_at_least(handle, n).ptr;
            }
            else
            {
                return generic_allocator_adapter::handle_allocate_conditional_zero(handle, n, true);
            }
        }

        inline static constexpr bool has_handle_reallocate{
            has_status && (::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_zero_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_zero_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_conditional_zero_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_conditional_zero_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_conditional_zero_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_conditional_zero_at_least_impl<alloc>)};

        inline static void* handle_reallocate_conditional_zero(handle_type handle, void* p, ::std::size_t n, bool zero) noexcept
            requires (has_status && has_handle_reallocate)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_conditional_zero_impl<alloc>)
            {
                return allocator_type::handle_reallocate_conditional_zero(handle, p, n, zero);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_conditional_zero_at_least_impl<alloc>)
            {
                return allocator_type::handle_reallocate_conditional_zero_at_least(handle, p, n, zero).ptr;
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_conditional_zero_impl<alloc>)
            {
                return allocator_type::handle_reallocate_aligned_conditional_zero(handle, p, default_alignment, n, zero);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_conditional_zero_at_least_impl<alloc>)
            {
                return allocator_type::handle_reallocate_aligned_conditional_zero_at_least(handle, p, default_alignment, n, zero).ptr;
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_impl<alloc> ||
                              ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_at_least_impl<alloc> ||
                              ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_impl<alloc> ||
                              ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_at_least_impl<alloc>)
            {
                if(zero)
                {
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate_zero(handle, p, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_at_least_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate_zero_at_least(handle, p, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_zero_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate_aligned_zero(handle, p, default_alignment, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_zero_at_least_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate_aligned_zero_at_least(handle, p, default_alignment, n).ptr;
                    }
                    else
                    {
                        ::fast_io::fast_terminate();
                    }
                }
                else
                {
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate(handle, p, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_at_least_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate_at_least(handle, p, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate_aligned(handle, p, default_alignment, n);
                    }
                    else
                    {
                        return allocator_type::handle_reallocate_aligned_at_least(handle, p, default_alignment, n).ptr;
                    }
                }
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_impl<alloc>)
                {
                    return allocator_type::handle_reallocate_zero(handle, p, n);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_at_least_impl<alloc>)
                {
                    return allocator_type::handle_reallocate_zero_at_least(handle, p, n).ptr;
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_zero_impl<alloc>)
                {
                    return allocator_type::handle_reallocate_aligned_zero(handle, p, default_alignment, n);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_zero_at_least_impl<alloc>)
                {
                    return allocator_type::handle_reallocate_aligned_zero_at_least(handle, p, default_alignment, n).ptr;
                }
                else
                {
                    ::fast_io::fast_terminate();
                }
            }
        }

        inline static void* handle_reallocate(handle_type handle, void* p, ::std::size_t n) noexcept
            requires (has_status && has_handle_reallocate)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_impl<alloc>)
            {
                return allocator_type::handle_reallocate(handle, p, n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_at_least_impl<alloc>)
            {
                return allocator_type::handle_reallocate_at_least(handle, p, n).ptr;
            }
            else
            {
                return generic_allocator_adapter::handle_reallocate_conditional_zero(handle, p, n, false);
            }
        }

        inline static void* handle_reallocate_zero(handle_type handle, void* p, ::std::size_t n) noexcept
            requires (has_status && has_handle_reallocate)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_impl<alloc>)
            {
                return allocator_type::handle_reallocate_zero(handle, p, n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_at_least_impl<alloc>)
            {
                return allocator_type::handle_reallocate_zero_at_least(handle, p, n).ptr;
            }
            else
            {
                return generic_allocator_adapter::handle_reallocate_conditional_zero(handle, p, n, true);
            }
        }

        inline static constexpr bool has_handle_reallocate_n{
            has_status && (::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_n_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_n_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_n_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_zero_n_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_n_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_n_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_n_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_zero_n_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_n_conditional_zero_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_n_conditional_zero_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_n_conditional_zero_at_least_impl<alloc> ||
                           ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_n_conditional_zero_at_least_impl<alloc>)};

        inline static void* handle_reallocate_n_conditional_zero(handle_type handle, void* p, ::std::size_t oldn, ::std::size_t n, bool zero) noexcept
            requires (has_status && has_handle_reallocate_n)
        {
            if(p != nullptr && oldn == n) { return p; }
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_n_conditional_zero_impl<alloc>)
            {
                return allocator_type::handle_reallocate_n_conditional_zero(handle, p, oldn, n, zero);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_n_conditional_zero_at_least_impl<alloc>)
            {
                return allocator_type::handle_reallocate_n_conditional_zero_at_least(handle, p, oldn, n, zero).ptr;
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_n_conditional_zero_impl<alloc>)
            {
                return allocator_type::handle_reallocate_aligned_n_conditional_zero(handle, p, oldn, default_alignment, n, zero);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_n_conditional_zero_at_least_impl<alloc>)
            {
                return allocator_type::handle_reallocate_aligned_n_conditional_zero_at_least(handle, p, oldn, default_alignment, n, zero).ptr;
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_n_impl<alloc> ||
                              ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_n_at_least_impl<alloc> ||
                              ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_n_impl<alloc> ||
                              ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_n_at_least_impl<alloc>)
            {
                if(zero)
                {
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_n_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate_zero_n(handle, p, oldn, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_n_at_least_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate_zero_n_at_least(handle, p, oldn, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_zero_n_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate_aligned_zero_n(handle, p, oldn, default_alignment, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_zero_n_at_least_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate_aligned_zero_n_at_least(handle, p, oldn, default_alignment, n).ptr;
                    }
                    else
                    {
                        auto newptr{generic_allocator_adapter::handle_reallocate_n(handle, p, oldn, n)};
                        if(newptr == nullptr) [[unlikely]] { return nullptr; }
                        if(oldn < n) { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(newptr) + oldn, n - oldn); }
                        return newptr;
                    }
                }
                else
                {
                    if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_n_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate_n(handle, p, oldn, n);
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_n_at_least_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate_n_at_least(handle, p, oldn, n).ptr;
                    }
                    else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_n_impl<alloc>)
                    {
                        return allocator_type::handle_reallocate_aligned_n(handle, p, oldn, default_alignment, n);
                    }
                    else
                    {
                        return allocator_type::handle_reallocate_aligned_n_at_least(handle, p, oldn, default_alignment, n).ptr;
                    }
                }
            }
            else
            {
                if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_n_impl<alloc>)
                {
                    return allocator_type::handle_reallocate_zero_n(handle, p, oldn, n);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_n_at_least_impl<alloc>)
                {
                    return allocator_type::handle_reallocate_zero_n_at_least(handle, p, oldn, n).ptr;
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_zero_n_impl<alloc>)
                {
                    return allocator_type::handle_reallocate_aligned_zero_n(handle, p, oldn, default_alignment, n);
                }
                else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_aligned_zero_n_at_least_impl<alloc>)
                {
                    return allocator_type::handle_reallocate_aligned_zero_n_at_least(handle, p, oldn, default_alignment, n).ptr;
                }
                else
                {
                    auto newptr{generic_allocator_adapter::handle_allocate(handle, n)};
                    if(newptr == nullptr) [[unlikely]] { return nullptr; }
                    if(p != nullptr)
                    {
                        if(n)
                        {
                            ::std::size_t copyn{oldn < n ? oldn : n};
                            ::fast_io::freestanding::nonoverlapped_bytes_copy_n(reinterpret_cast<::std::byte const*>(p),
                                                                                copyn,
                                                                                reinterpret_cast<::std::byte*>(newptr));
                        }
                        generic_allocator_adapter::handle_deallocate_n(handle, p, oldn);
                    }
                    if(zero && oldn < n) { ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(newptr) + oldn, n - oldn); }
                    return newptr;
                }
            }
        }

        inline static void* handle_reallocate_n(handle_type handle, void* p, ::std::size_t oldn, ::std::size_t n) noexcept
            requires (has_status && has_handle_reallocate_n)
        {
            if(p != nullptr && oldn == n) { return p; }
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_n_impl<alloc>)
            {
                return allocator_type::handle_reallocate_n(handle, p, oldn, n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_n_at_least_impl<alloc>)
            {
                return allocator_type::handle_reallocate_n_at_least(handle, p, oldn, n).ptr;
            }
            else
            {
                return generic_allocator_adapter::handle_reallocate_n_conditional_zero(handle, p, oldn, n, false);
            }
        }

        inline static void* handle_reallocate_zero_n(handle_type handle, void* p, ::std::size_t oldn, ::std::size_t n) noexcept
            requires (has_status && has_handle_reallocate_n)
        {
            if(p != nullptr && oldn == n) { return p; }
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_n_impl<alloc>)
            {
                return allocator_type::handle_reallocate_zero_n(handle, p, oldn, n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_reallocate_zero_n_at_least_impl<alloc>)
            {
                return allocator_type::handle_reallocate_zero_n_at_least(handle, p, oldn, n).ptr;
            }
            else
            {
                return generic_allocator_adapter::handle_reallocate_n_conditional_zero(handle, p, oldn, n, true);
            }
        }

        inline static constexpr bool has_handle_deallocate{has_status &&
                                                           (::uwvm2::utils::allocator::fast_io_strict::details::has_handle_deallocate_impl<alloc> ||
                                                            ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_deallocate_aligned_impl<alloc> ||
                                                            ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_deallocate_n_impl<alloc> ||
                                                            ::uwvm2::utils::allocator::fast_io_strict::details::has_handle_deallocate_aligned_n_impl<alloc>)};

        inline static void handle_deallocate(handle_type handle, void* p) noexcept
            requires (has_status && has_handle_deallocate)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_deallocate_impl<alloc>)
            {
                allocator_type::handle_deallocate(handle, p);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_deallocate_aligned_impl<alloc>)
            {
                allocator_type::handle_deallocate_aligned(handle, p, default_alignment);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_deallocate_n_impl<alloc>)
            {
                allocator_type::handle_deallocate_n(handle, p, 0);
            }
            else
            {
                allocator_type::handle_deallocate_aligned_n(handle, p, default_alignment, 0);
            }
        }

        inline static void handle_deallocate_n(handle_type handle, void* p, ::std::size_t n) noexcept
            requires (has_status && has_handle_deallocate)
        {
            if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_deallocate_n_impl<alloc>)
            {
                allocator_type::handle_deallocate_n(handle, p, n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_deallocate_aligned_n_impl<alloc>)
            {
                allocator_type::handle_deallocate_aligned_n(handle, p, default_alignment, n);
            }
            else if constexpr(::uwvm2::utils::allocator::fast_io_strict::details::has_handle_deallocate_impl<alloc>)
            {
                allocator_type::handle_deallocate(handle, p);
            }
            else
            {
                allocator_type::handle_deallocate_aligned(handle, p, default_alignment);
            }
        }
    };

    template <typename alloc, typename T>
    class fast_io_strict_typed_generic_allocator_adapter
    {
    public:
        using allocator_adaptor = alloc;
        inline static constexpr bool has_status{allocator_adaptor::has_status};
        using handle_type = typename allocator_adaptor::handle_type;
        inline static
#if __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif
            T* allocate(::std::size_t n) noexcept
            requires (!has_status)
        {
#if __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated()) { return ::fast_io::freestanding::allocator<T>{}.allocate(n); }
#endif
            constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
            if(n > mxn) { ::fast_io::fast_terminate(); }
            if constexpr(alignof(T) <= alloc::default_alignment) { return static_cast<T*>(alloc::allocate(n * sizeof(T))); }
            else
            {
                return static_cast<T*>(alloc::allocate_aligned(alignof(T), n * sizeof(T)));
            }
        }

        inline static
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif

            ::fast_io::basic_allocation_least_result<T*> allocate_at_least(::std::size_t n) noexcept
            requires (!has_status)
        {
#if __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated()) { return {::fast_io::freestanding::allocator<T>{}.allocate(n), n}; }
#endif
            constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
            if(n > mxn) { ::fast_io::fast_terminate(); }
            if constexpr(alignof(T) <= alloc::default_alignment)
            {
                auto newres{alloc::allocate_at_least(n * sizeof(T))};
                return {reinterpret_cast<T*>(newres.ptr), newres.count / sizeof(T)};
            }
            else
            {
                auto newres{alloc::allocate_aligned_at_least(alignof(T), n * sizeof(T))};
                return {reinterpret_cast<T*>(newres.ptr), newres.count / sizeof(T)};
            }
        }

        inline static
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif

            T* allocate_zero(::std::size_t n) noexcept
            requires (!has_status)
        {
            constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
            if(n > mxn) { ::fast_io::fast_terminate(); }
            if constexpr(alignof(T) <= alloc::default_alignment) { return static_cast<T*>(alloc::allocate_zero(n * sizeof(T))); }
            else
            {
                return static_cast<T*>(alloc::allocate_aligned_zero(alignof(T), n * sizeof(T)));
            }
        }

        inline static
#if __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif
            ::fast_io::basic_allocation_least_result<T*> allocate_zero_at_least(::std::size_t n) noexcept
            requires (!has_status)
        {
#if __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated()) { return {::fast_io::freestanding::allocator<T>{}.allocate(n), n}; }
#endif
            constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
            if(n > mxn) { ::fast_io::fast_terminate(); }
            if constexpr(alignof(T) <= alloc::default_alignment)
            {
                auto newres{alloc::allocate_zero_at_least(n * sizeof(T))};
                return {reinterpret_cast<T*>(newres.ptr), newres.count / sizeof(T)};
            }
            else
            {
                auto newres{alloc::allocate_aligned_zero_at_least(alignof(T), n * sizeof(T))};
                return {reinterpret_cast<T*>(newres.ptr), newres.count / sizeof(T)};
            }
        }

        inline static constexpr bool has_reallocate = allocator_adaptor::has_reallocate;

        inline static
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif
            T* reallocate(T* ptr, ::std::size_t n) noexcept
            requires (!has_status && has_reallocate)
        {
            constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
            if(n > mxn) { ::fast_io::fast_terminate(); }
            if constexpr(alignof(T) <= alloc::default_alignment) { return static_cast<T*>(alloc::reallocate(ptr, n * sizeof(T))); }
            else
            {
                return static_cast<T*>(alloc::reallocate_aligned(ptr, alignof(T), n * sizeof(T)));
            }
        }

        inline static
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif
            ::fast_io::basic_allocation_least_result<T*> reallocate_at_least(T* ptr, ::std::size_t n) noexcept
            requires (!has_status && has_reallocate)
        {
            constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
            if(n > mxn) { ::fast_io::fast_terminate(); }
            if constexpr(alignof(T) <= alloc::default_alignment)
            {
                auto newres{alloc::reallocate_at_least(ptr, n * sizeof(T))};
                return {reinterpret_cast<T*>(newres.ptr), newres.count / sizeof(T)};
            }
            else
            {
                auto newres{alloc::reallocate_aligned_at_least(ptr, alignof(T), n * sizeof(T))};
                return {reinterpret_cast<T*>(newres.ptr), newres.count / sizeof(T)};
            }
        }

        inline static constexpr bool has_reallocate_zero = allocator_adaptor::has_reallocate_zero;
        inline static
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif
            T* reallocate_zero(T* ptr, ::std::size_t n) noexcept
            requires (!has_status && has_reallocate_zero)
        {
            constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
            if(n > mxn) { ::fast_io::fast_terminate(); }
            if constexpr(alignof(T) <= alloc::default_alignment) { return static_cast<T*>(alloc::reallocate_zero(ptr, n * sizeof(T))); }
            else
            {
                return static_cast<T*>(alloc::reallocate_aligned_zero(ptr, alignof(T), n * sizeof(T)));
            }
        }

        inline static
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif
            ::fast_io::basic_allocation_least_result<T*> reallocate_zero_at_least(T* ptr, ::std::size_t n) noexcept
            requires (!has_status && has_reallocate_zero)
        {
            constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
            if(n > mxn) { ::fast_io::fast_terminate(); }
            if constexpr(alignof(T) <= alloc::default_alignment)
            {
                auto newres{alloc::reallocate_zero_at_least(ptr, n * sizeof(T))};
                return {reinterpret_cast<T*>(newres.ptr), newres.count / sizeof(T)};
            }
            else
            {
                auto newres{alloc::reallocate_aligned_zero_at_least(ptr, alignof(T), n * sizeof(T))};
                return {reinterpret_cast<T*>(newres.ptr), newres.count / sizeof(T)};
            }
        }

        inline static
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif
            T* reallocate_n(T* ptr, ::std::size_t oldn, ::std::size_t n) noexcept
            requires (!has_status)
        {
            constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
            if(n > mxn) { ::fast_io::fast_terminate(); }
            if constexpr(alignof(T) <= alloc::default_alignment) { return static_cast<T*>(alloc::reallocate_n(ptr, oldn * sizeof(T), n * sizeof(T))); }
            else
            {
                return static_cast<T*>(alloc::reallocate_aligned_n(ptr, oldn * sizeof(T), alignof(T), n * sizeof(T)));
            }
        }

        inline static
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif
            ::fast_io::basic_allocation_least_result<T*> reallocate_n_at_least(T* ptr, ::std::size_t oldn, ::std::size_t n) noexcept
            requires (!has_status)
        {
            constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
            if(n > mxn) { ::fast_io::fast_terminate(); }
            if constexpr(alignof(T) <= alloc::default_alignment)
            {
                auto newres{alloc::reallocate_n_at_least(ptr, oldn * sizeof(T), n * sizeof(T))};
                return {reinterpret_cast<T*>(newres.ptr), newres.count / sizeof(T)};
            }
            else
            {
                auto newres{alloc::reallocate_aligned_n_at_least(ptr, oldn * sizeof(T), alignof(T), n * sizeof(T))};
                return {reinterpret_cast<T*>(newres.ptr), newres.count / sizeof(T)};
            }
        }

        inline static
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif
            T* reallocate_zero_n(T* ptr, ::std::size_t oldn, ::std::size_t n) noexcept
            requires (!has_status)
        {
            constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
            if(n > mxn) { ::fast_io::fast_terminate(); }
            if constexpr(alignof(T) <= alloc::default_alignment) { return static_cast<T*>(alloc::reallocate_zero_n(ptr, oldn * sizeof(T), n * sizeof(T))); }
            else
            {
                return static_cast<T*>(alloc::reallocate_aligned_zero_n(ptr, oldn * sizeof(T), alignof(T), n * sizeof(T)));
            }
        }

        inline static
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif
            ::fast_io::basic_allocation_least_result<T*> reallocate_zero_n_at_least(T* ptr, ::std::size_t oldn, ::std::size_t n) noexcept
            requires (!has_status)
        {
            constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
            if(n > mxn) { ::fast_io::fast_terminate(); }
            if constexpr(alignof(T) <= alloc::default_alignment)
            {
                auto newres{alloc::reallocate_zero_n_at_least(ptr, oldn * sizeof(T), n * sizeof(T))};
                return {reinterpret_cast<T*>(newres.ptr), newres.count / sizeof(T)};
            }
            else
            {
                auto newres{alloc::reallocate_aligned_zero_n_at_least(ptr, oldn * sizeof(T), alignof(T), n * sizeof(T))};
                return {reinterpret_cast<T*>(newres.ptr), newres.count / sizeof(T)};
            }
        }

        inline static constexpr bool has_deallocate = allocator_adaptor::has_deallocate;

        inline static
#if __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif
            void deallocate(T* ptr) noexcept
            requires (!has_status && has_deallocate)
        {
#if __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
            {
                if(ptr) { return ::fast_io::freestanding::allocator<T>{}.deallocate(ptr, 1); }
                else
                {
                    return;
                }
            }
#endif
            if constexpr(alignof(T) <= alloc::default_alignment) { return alloc::deallocate(ptr); }
            else
            {
                return alloc::deallocate_aligned(ptr, alignof(T));
            }
        }

        inline static
#if __cpp_constexpr_dynamic_alloc >= 201907L
            constexpr
#endif
            void deallocate_n(T* ptr, ::std::size_t n) noexcept
            requires (!has_status)
        {
#if __cpp_constexpr_dynamic_alloc >= 201907L
            if(__builtin_is_constant_evaluated())
            {
                if(ptr) { return ::fast_io::freestanding::allocator<T>{}.deallocate(ptr, n); }
                else
                {
                    return;
                }
            }
#endif
            if constexpr(alignof(T) <= alloc::default_alignment) { alloc::deallocate_n(ptr, n * sizeof(T)); }
            else
            {
                alloc::deallocate_aligned_n(ptr, alignof(T), n * sizeof(T));
            }
        }

#if 0
	static inline
# if __cpp_constexpr_dynamic_alloc >= 201907L
		constexpr
# endif
		T *
		handle_allocate(handle_type handle, ::std::size_t n) noexcept
		requires(has_status)
	{
# if __cpp_constexpr_dynamic_alloc >= 201907L
		if (__builtin_is_constant_evaluated())
		{
			if (n)
			{
				return ::fast_io::freestanding::allocator<T>{}.allocate(n);
			}
			else
			{
				return nullptr;
			}
		}
# endif
		constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
		if (n > mxn)
		{
			::fast_io::fast_terminate();
		}
		if constexpr (alignof(T) <= alloc::default_alignment)
		{
			return static_cast<T *>(alloc::handle_allocate(handle, n * sizeof(T)));
		}
		else
		{
			return static_cast<T *>(alloc::handle_allocate_aligned(handle, alignof(T), n * sizeof(T)));
		}
	}

	static inline
# if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
		constexpr
# endif
		T *
		handle_allocate_zero(handle_type handle, ::std::size_t n) noexcept
		requires(has_status)
	{
		constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
		if (n > mxn)
		{
			::fast_io::fast_terminate();
		}
		if constexpr (alignof(T) <= alloc::default_alignment)
		{
			return static_cast<T *>(alloc::handle_allocate_zero(handle, n * sizeof(T)));
		}
		else
		{
			return static_cast<T *>(alloc::handle_allocate_zero_aligned(handle, alignof(T), n * sizeof(T)));
		}
	}

	static inline constexpr bool has_handle_reallocate = allocator_adaptor::has_handle_reallocate;
	static inline
# if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
		constexpr
# endif
		T *
		handle_reallocate(handle_type handle, T *ptr, ::std::size_t n) noexcept
		requires(has_status && has_handle_reallocate)
	{
		constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
		if (n > mxn)
		{
			::fast_io::fast_terminate();
		}
		if constexpr (alignof(T) <= alloc::default_alignment)
		{
			return static_cast<T *>(alloc::handle_reallocate(handle, ptr, n * sizeof(T)));
		}
		else
		{
			return static_cast<T *>(alloc::handle_reallocate_aligned(handle, ptr, alignof(T), n * sizeof(T)));
		}
	}

	static inline constexpr bool has_handle_reallocate_zero = allocator_adaptor::has_handle_reallocate_zero;

	static inline
# if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
		constexpr
# endif
		T *
		handle_reallocate_zero(handle_type handle, T *ptr, ::std::size_t n) noexcept
		requires(has_status && has_handle_reallocate)
	{
		constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
		if (n > mxn)
		{
			::fast_io::fast_terminate();
		}
		if constexpr (alignof(T) <= alloc::default_alignment)
		{
			return static_cast<T *>(alloc::handle_reallocate_zero(handle, ptr, n * sizeof(T)));
		}
		else
		{
			return static_cast<T *>(alloc::handle_reallocate_aligned_zero(handle, ptr, alignof(T), n * sizeof(T)));
		}
	}

	static inline
# if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
		constexpr
# endif
		T *
		handle_reallocate_n(handle_type handle, T *ptr, ::std::size_t oldn, ::std::size_t n) noexcept
		requires(has_status)
	{
		constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
		if (n > mxn)
		{
			::fast_io::fast_terminate();
		}
		if constexpr (alignof(T) <= alloc::default_alignment)
		{
			return static_cast<T *>(alloc::handle_reallocate_n(handle, ptr, oldn * sizeof(T), n * sizeof(T)));
		}
		else
		{
			return static_cast<T *>(alloc::handle_reallocate_aligned_n(handle, ptr, oldn * sizeof(T), alignof(T), n * sizeof(T)));
		}
	}

	static inline
# if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
		constexpr
# endif
		T *
		handle_reallocate_zero_n(handle_type handle, T *ptr, ::std::size_t oldn, ::std::size_t n) noexcept
		requires(has_status)
	{
		constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / sizeof(T)};
		if (n > mxn)
		{
			::fast_io::fast_terminate();
		}
		if constexpr (alignof(T) <= alloc::default_alignment)
		{
			return static_cast<T *>(alloc::handle_reallocate_zero_n(handle, ptr, oldn * sizeof(T), n * sizeof(T)));
		}
		else
		{
			return static_cast<T *>(alloc::handle_reallocate_aligned_zero_n(handle, ptr, oldn * sizeof(T), alignof(T), n * sizeof(T)));
		}
	}

	static inline constexpr bool has_handle_deallocate = allocator_adaptor::has_handle_deallocate;
	static inline
# if __cpp_constexpr_dynamic_alloc >= 201907L
		constexpr
# endif
		void
		handle_deallocate(handle_type handle, T *ptr) noexcept
		requires(has_status && has_handle_deallocate)
	{
# if __cpp_constexpr_dynamic_alloc >= 201907L
		if (__builtin_is_constant_evaluated())
		{
			if (ptr)
			{
				return ::fast_io::freestanding::allocator<T>{}.deallocate(ptr, 1);
			}
			else
			{
				return;
			}
		}
# endif
		if constexpr (alignof(T) <= alloc::default_alignment)
		{
			return alloc::handle_deallocate(handle, ptr);
		}
		else
		{
			return alloc::handle_deallocate_aligned(handle, ptr, alignof(T));
		}
	}

	static inline
# if __cpp_constexpr_dynamic_alloc >= 201907L
		constexpr
# endif
		void
		handle_deallocate_n(handle_type handle, T *ptr, ::std::size_t n) noexcept
		requires(has_status)
	{
# if __cpp_constexpr_dynamic_alloc >= 201907L
		if (__builtin_is_constant_evaluated())
		{
			if (ptr)
			{
				return ::fast_io::freestanding::allocator<T>{}.deallocate(ptr, n);
			}
			else
			{
				return;
			}
		}
# endif
		if constexpr (alignof(T) <= alloc::default_alignment)
		{
			alloc::handle_deallocate_n(handle, ptr, n * sizeof(T));
		}
		else
		{
			alloc::handle_deallocate_aligned_n(handle, ptr, alignof(T), n * sizeof(T));
		}
	}
#endif
    };

    namespace details
    {

        template <typename alloc>
        inline constexpr void* allocator_pointer_aligned_impl(::std::size_t alignment, ::std::size_t n, bool zero) noexcept
        {
            static_assert(::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_generic_allocator_adapter<alloc>::has_native_allocate);

            constexpr ::std::size_t defaultalignment{::uwvm2::utils::allocator::fast_io_strict::details::calculate_default_alignment<alloc>()};
            bool const alignedadjustment{defaultalignment < alignment};
            if(alignedadjustment) { n = ::uwvm2::utils::allocator::fast_io_strict::details::allocator_compute_aligned_total_size_impl(alignment, n); }
            void* p = ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_generic_allocator_adapter<alloc>::allocate_conditional_zero(n, zero);
            if(alignedadjustment) { p = ::uwvm2::utils::allocator::fast_io_strict::details::allocator_adjust_ptr_to_aligned_impl(p, alignment); }
            return p;
        }

        template <typename alloc>
        inline constexpr ::fast_io::allocation_least_result
            allocator_pointer_aligned_at_least_impl(::std::size_t alignment, ::std::size_t n, bool zero) noexcept
        {
            static_assert(::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_generic_allocator_adapter<alloc>::has_native_allocate);

            constexpr ::std::size_t defaultalignment{::uwvm2::utils::allocator::fast_io_strict::details::calculate_default_alignment<alloc>()};
            bool const alignedadjustment{defaultalignment < alignment};
            if(alignedadjustment) { n = ::uwvm2::utils::allocator::fast_io_strict::details::allocator_compute_aligned_total_size_impl(alignment, n); }
            ::fast_io::allocation_least_result res =
                ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_generic_allocator_adapter<alloc>::allocate_conditional_zero_at_least(n, zero);
            if(alignedadjustment)
            {
                auto resptr{res.ptr};
                if(resptr == nullptr) [[unlikely]] { return {nullptr, 0}; }
                auto aligned_ptr = ::uwvm2::utils::allocator::fast_io_strict::details::allocator_adjust_ptr_to_aligned_impl(resptr, alignment);
                if(aligned_ptr == nullptr) [[unlikely]] { return {nullptr, 0}; }
                auto const diff{static_cast<::std::size_t>(reinterpret_cast<char unsigned*>(aligned_ptr) - reinterpret_cast<char unsigned*>(resptr))};
                if(res.count < diff) [[unlikely]] { return {nullptr, 0}; }
                res = {aligned_ptr, res.count - diff};
            }
            return res;
        }

#if 0
template <typename alloc, bool zero>
inline constexpr void *status_allocator_pointer_aligned_impl(typename alloc::handle_type handle, ::std::size_t alignment, ::std::size_t n) noexcept
{
	static_assert(::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_generic_allocator_adapter<alloc>::has_native_handle_allocate);

	constexpr ::std::size_t defaultalignment{::uwvm2::utils::allocator::fast_io_strict::details::calculate_default_alignment<alloc>()};
	bool const alignedadjustment{defaultalignment < alignment};
	if (alignedadjustment)
	{
		n = ::uwvm2::utils::allocator::fast_io_strict::details::allocator_compute_aligned_total_size_impl(alignment, n);
	}
	void *p;
	if constexpr (zero)
	{
		p = ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_generic_allocator_adapter<alloc>::handle_allocate_zero(handle, n);
	}
	else
	{
		p = ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_generic_allocator_adapter<alloc>::handle_allocate(handle, n);
	}
	if (alignedadjustment)
	{
		p = ::uwvm2::utils::allocator::fast_io_strict::details::allocator_adjust_ptr_to_aligned_impl(p,alignment);
	}
	return p;
}

template <typename alloc, bool zero>
inline constexpr ::fast_io::allocation_least_result status_allocator_pointer_aligned_impl(typename alloc::handle_type handle, ::std::size_t alignment, ::std::size_t n) noexcept
{
	static_assert(::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_generic_allocator_adapter<alloc>::has_native_handle_allocate);

	constexpr ::std::size_t defaultalignment{::uwvm2::utils::allocator::fast_io_strict::details::calculate_default_alignment<alloc>()};
	bool const alignedadjustment{defaultalignment < alignment};
	if (alignedadjustment)
	{
		n = ::uwvm2::utils::allocator::fast_io_strict::details::allocator_compute_aligned_total_size_impl(alignment, n);
	}
	::fast_io::allocation_least_result res;
	if constexpr (zero)
	{
		res = ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_generic_allocator_adapter<alloc>::handle_allocate_zero_at_least(handle, n);
	}
	else
	{
		res = ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_generic_allocator_adapter<alloc>::handle_allocate_at_least(handle, n);
	}
	if (alignedadjustment)
	{
		auto resptr{res.ptr};
		auto aligned_ptr = ::uwvm2::utils::allocator::fast_io_strict::details::allocator_adjust_ptr_to_aligned_impl(resptr,alignment);
		res = {aligned_ptr, res.count - static_cast<::std::size_t>(reinterpret_cast<char unsigned *>(aligned_ptr) - reinterpret_cast<char unsigned *>(resptr))};
	}
	return res;
}
#endif
    }  // namespace details

}  // namespace uwvm2::utils::allocator::fast_io_strict

#pragma pop_macro("UWVM2_FAST_IO_STRICT_INTERNAL_HAS_BUILTIN")
