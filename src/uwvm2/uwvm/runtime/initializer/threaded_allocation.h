// Cold, explicit child-construction allocation accounting. SOURCE ONLY.
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace uwvm2::uwvm::runtime::initializer
{
    // Counts actual requested allocation bytes, including control blocks and
    // compiler-supplied new[] cookie bytes. This is not RSS or malloc overhead.
    // The count is monotonic through construction (freed scratch is not reused
    // as an excuse to exceed a per-child request budget).
    class threaded_allocation_account final
    {
        ::std::atomic_size_t references_{1uz};
        ::std::atomic_size_t requested_{};
        ::std::size_t maximum_{};
        explicit threaded_allocation_account(::std::size_t maximum) noexcept
            : requested_{sizeof(threaded_allocation_account)}, maximum_{maximum} {}
    public:
        [[nodiscard]] static threaded_allocation_account* create(::std::size_t maximum) noexcept
        {
            if(maximum < sizeof(threaded_allocation_account)) { return nullptr; }
            return new(::std::nothrow) threaded_allocation_account{maximum};
        }
        void retain() noexcept { references_.fetch_add(1uz, ::std::memory_order_relaxed); }
        void release() noexcept
        { if(references_.fetch_sub(1uz, ::std::memory_order_acq_rel) == 1uz) { delete this; } }
        [[nodiscard]] bool charge(::std::size_t bytes) noexcept
        {
            auto previous{requested_.load(::std::memory_order_relaxed)};
            for(;;)
            {
                if(bytes > maximum_ - previous) { return false; }
                if(requested_.compare_exchange_weak(previous, previous + bytes,
                    ::std::memory_order_relaxed, ::std::memory_order_relaxed)) { return true; }
            }
        }
        [[nodiscard]] ::std::size_t requested_bytes() const noexcept
        { return requested_.load(::std::memory_order_relaxed); }
        [[nodiscard]] void* try_malloc(::std::size_t bytes) noexcept
        {
            if(bytes == 0uz) { return nullptr; }
            return charge(bytes) ? ::std::malloc(bytes) : nullptr;
        }
        [[nodiscard]] void* try_new_array(::std::size_t actual_bytes) noexcept
        { return charge(actual_bytes) ? ::operator new[](actual_bytes, ::std::nothrow) : nullptr; }
        [[nodiscard]] void* try_new_array(::std::size_t actual_bytes, ::std::align_val_t alignment) noexcept
        { return charge(actual_bytes) ? ::operator new[](actual_bytes, alignment, ::std::nothrow) : nullptr; }
    };

    class threaded_allocation_pin final
    {
        threaded_allocation_account* account_{};
        struct adopt_tag {};
        explicit threaded_allocation_pin(threaded_allocation_account* value, adopt_tag) noexcept : account_{value} {}
    public:
        [[nodiscard]] static threaded_allocation_pin create(::std::size_t maximum) noexcept
        { return threaded_allocation_pin{threaded_allocation_account::create(maximum), adopt_tag{}}; }
        threaded_allocation_pin() noexcept = default;
        threaded_allocation_pin(threaded_allocation_pin const& other) noexcept : account_{other.account_}
        { if(account_ != nullptr) { account_->retain(); } }
        threaded_allocation_pin(threaded_allocation_pin&& other) noexcept : account_{::std::exchange(other.account_, nullptr)} {}
        threaded_allocation_pin& operator=(threaded_allocation_pin other) noexcept
        { ::std::swap(account_, other.account_); return *this; }
        ~threaded_allocation_pin() { if(account_ != nullptr) { account_->release(); } }
        [[nodiscard]] explicit operator bool() const noexcept { return account_ != nullptr; }
        [[nodiscard]] threaded_allocation_account& get() const noexcept { return *account_; }
    };

    template<typename T> class threaded_std_allocator
    {
        template<typename> friend class threaded_std_allocator;
        threaded_allocation_pin pin_;
    public:
        using value_type = T;
        explicit threaded_std_allocator(threaded_allocation_pin pin) noexcept : pin_{::std::move(pin)} {}
        template<typename U> threaded_std_allocator(threaded_std_allocator<U> const& other) noexcept : pin_{other.pin_} {}
        [[nodiscard]] T* allocate(::std::size_t count)
        {
            void* raw{};
            if(count <= (::std::numeric_limits<::std::size_t>::max)() / sizeof(T) && pin_)
            {
                auto const bytes{count * sizeof(T)};
                if(pin_.get().charge(bytes))
                {
                    if constexpr(alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
                    { raw = ::operator new(bytes, ::std::align_val_t{alignof(T)}, ::std::nothrow); }
                    else { raw = ::operator new(bytes, ::std::nothrow); }
                }
            }
            if(raw == nullptr)
            {
#ifdef UWVM_CPP_EXCEPTIONS
                throw ::std::bad_alloc{};
#else
                ::std::terminate(); // create() rejects this profile before any allocation.
#endif
            }
            return static_cast<T*>(raw);
        }
        void deallocate(T* pointer, ::std::size_t) noexcept
        {
            if constexpr(alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            { ::operator delete(pointer, ::std::align_val_t{alignof(T)}); }
            else { ::operator delete(pointer); }
        }
        template<typename U, typename... Args> void construct(U* pointer, Args&&... args)
        { ::new(const_cast<void*>(static_cast<void const*>(pointer))) U(::std::forward<Args>(args)...); }
        template<typename U> [[nodiscard]] bool operator==(threaded_std_allocator<U> const& other) const noexcept
        { return ::std::addressof(pin_.get()) == ::std::addressof(other.pin_.get()); }
    };

    // The placement allocation overload sees the exact ABI request, including
    // a nontrivial element's array cookie; matching delete[] stays standard.
    struct threaded_nothrow_array { threaded_allocation_account* account; };
}

[[nodiscard]] inline void* operator new[](::std::size_t bytes,
    ::uwvm2::uwvm::runtime::initializer::threaded_nothrow_array request) noexcept
{ return request.account->try_new_array(bytes); }
inline void operator delete[](void* pointer,
    ::uwvm2::uwvm::runtime::initializer::threaded_nothrow_array) noexcept
{ ::operator delete[](pointer); }
[[nodiscard]] inline void* operator new[](::std::size_t bytes, ::std::align_val_t alignment,
    ::uwvm2::uwvm::runtime::initializer::threaded_nothrow_array request) noexcept
{ return request.account->try_new_array(bytes, alignment); }
inline void operator delete[](void* pointer, ::std::align_val_t alignment,
    ::uwvm2::uwvm::runtime::initializer::threaded_nothrow_array) noexcept
{ ::operator delete[](pointer, alignment); }
