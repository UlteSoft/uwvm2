// Protected call opfunc qualification with real dispatch bytecode. Validator emission is not
// connected yet: this tests the execution/ownership ABI, not end-to-end Wasm EH coverage.
#include <uwvm2/runtime/compiler/uwvm_int/optable/exception.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#if defined(_WIN32) && defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
# define TEST_OP_ABI __attribute__((sysv_abi))
#elif defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
# define TEST_OP_ABI __attribute__((fastcall))
#elif defined(_M_IX86)
# define TEST_OP_ABI __fastcall
#else
# define TEST_OP_ABI
#endif

namespace op = uwvm2::runtime::compiler::uwvm_int::optable;
namespace exn = uwvm2::runtime::exception;
namespace
{
    unsigned checks{}, cases{};
#define CHECK(condition) do { ++checks; if(!(condition)) { std::fprintf(stderr, "FAIL %u: %s\n", __LINE__, #condition); std::abort(); } } while(false)

    using slot = op::wasm_stack_top_i32_i64_f32_f64_u;
    enum class behavior { normal, matching, nonmatching, foreign };
    struct foreign_error { unsigned code; };
    constexpr std::uint64_t payload_bits{0xfff0123456789abcULL};
    constexpr std::int64_t first_cached{0x123456789abcdefLL};
    constexpr std::int64_t second_cached{-0x23456789abcdefLL};

    struct state
    {
        behavior action{};
        bool indirect{};
        unsigned slots{};
        unsigned called{}, dispatched{}, resumed{};
        std::array<std::byte, 128> frame{};
        std::array<std::byte, 64> local_storage{};
        std::byte const* expected_ip{};
        std::byte const* handler_ip{};
        std::byte* expected_top{};
        void const* accepted_tag{};
        void const* payload_identity{};
        exn::value const* exception_identity{};
        std::weak_ptr<int> weak_tag{}, weak_payload{};
        exn::value_ref pending{}, retained{};

        std::byte* original_top() noexcept { return frame.data() + 80; }
        std::byte const* locals() noexcept { return local_storage.data() + 1 + sizeof(void*); }
        void check_prefix() noexcept
        {
            for(std::size_t i{}; i != 32; ++i) { CHECK(frame[i] == static_cast<std::byte>(i + 1)); }
            void const* saved{};
            std::memcpy(&saved, local_storage.data() + 1, sizeof(saved));
            CHECK(saved == this);
            for(std::size_t i{1 + sizeof(void*)}; i != local_storage.size(); ++i)
            { CHECK(local_storage[i] == std::byte{0x63}); }
        }
    };
    state* active{};

    std::byte* callee(std::byte* top)
    {
        CHECK(top == active->original_top());
        ++active->called;
        active->check_prefix();
        // A callee may overwrite popped arguments. It must not overwrite the preserved caller prefix.
        std::memset(active->frame.data() + 32, 0xa5, 48);
        switch(active->action)
        {
            case behavior::normal:
                std::memcpy(top - 16, &payload_bits, sizeof(payload_bits));
                return top - 8;
            case behavior::matching: case behavior::nonmatching:
                exn::throw_value(std::move(active->pending));
            case behavior::foreign:
                throw foreign_error{731};
        }
        std::abort();
    }

    std::byte* TEST_OP_ABI direct(std::size_t module, std::size_t function, std::byte* top)
    {
        CHECK(!active->indirect && module == 7 && function == 19);
        return callee(top);
    }

    std::byte* TEST_OP_ABI indirect(std::size_t module, std::size_t type, std::size_t table, std::byte* top)
    {
        CHECK(active->indirect && module == 7 && type == 23 && table == 29);
        return callee(top);
    }

    op::exception_continuation dispatch(void const* context, exn::value_ref const& caught,
                                        std::byte const* locals, std::byte* top) noexcept
    {
        CHECK(context == active);
        CHECK(locals == active->locals() && top == active->original_top());
        CHECK(caught && caught.get() == active->exception_identity && caught.use_count() == 1);
        CHECK(!active->pending);
        ++active->dispatched;
        active->check_prefix();
        if(caught->tag_identity() != active->accepted_tag) { return {}; }
        CHECK(active->action == behavior::matching);
        CHECK(caught->fields().size() == 2);
        CHECK(caught->fields()[0].kind() == exn::payload_kind::f64);
        CHECK(caught->fields()[1].root().get() == active->payload_identity);
        // catch_ref must own the instance before the native catch ends. Its payload roots then keep
        // the reference-valued argument alive as well; copying its pointer bits would not suffice.
        active->retained = caught;
        CHECK(caught.use_count() == 2);
        auto restored = active->frame.data() + 32;
        std::memcpy(restored, caught->fields()[0].bits().data(), sizeof(payload_bits));
        restored += sizeof(payload_bits);
        // Only the numeric payload is materialized in this synthetic stack. Reference/catch_ref
        // roots stay in retained; canonical pointer-width bits are not the current wasm_global_ref_t
        // carrier. A backend reference-kind/type codec must reconstruct that ABI in future emission.
        CHECK(restored == active->expected_top);
        return {active->handler_ip, restored};
    }

    void terminal(std::byte const* ip, std::byte* top, std::byte const* locals) noexcept
    {
        ++active->resumed;
        CHECK(ip == active->expected_ip && top == active->expected_top && locals == active->locals());
        active->check_prefix();
        if(active->action == behavior::normal)
        {
            CHECK(active->dispatched == 0 && !active->retained);
            std::uint64_t result{};
            std::memcpy(&result, top - sizeof(result), sizeof(result));
            CHECK(result == payload_bits);
        }
        else
        {
            CHECK(active->action == behavior::matching && active->dispatched == 1);
            // The native activation no longer owns this instance: __cxa_end_catch must have completed
            // BEFORE the next opfunc runs, including when dispatch is a musttail indirect jump.
            CHECK(active->retained && active->retained.use_count() == 1);
            CHECK(!active->weak_tag.expired() && !active->weak_payload.expired());
            std::uint64_t number{};
            std::memcpy(&number, active->frame.data() + 32, sizeof(number));
            CHECK(number == payload_bits);
            CHECK(active->retained->fields()[1].root().get() == active->payload_identity);
            CHECK(active->retained.get() == active->exception_identity);
        }
    }

    template<class... Cached>
    [[gnu::noinline]] void TEST_OP_ABI finish(std::byte const* ip, std::byte* top,
                                             std::byte const* locals, Cached... cached)
    {
        CHECK(sizeof...(Cached) == active->slots);
        if constexpr(sizeof...(Cached) >= 1) { CHECK(cached...[0].i64 == first_cached); }
        if constexpr(sizeof...(Cached) >= 2) { CHECK(cached...[1].i64 == second_cached); }
        // Cache ABI bits merely survive this opcode. A real handler's logical cache is empty and
        // reloads payload values from the restored stack; these stale bits are not the catch payload.
        terminal(ip, top, locals);
    }

    void TEST_OP_ABI finish_byref(std::byte const*& ip, std::byte*& top, std::byte const*& locals)
    { terminal(ip, top, locals); ip = nullptr; }

    template<unsigned Slots, bool Tail>
    consteval op::uwvm_interpreter_translate_option_t option()
    {
        op::uwvm_interpreter_translate_option_t result{.is_tail_call = Tail};
        if constexpr(Slots != 0)
        {
            static_assert(Tail);
            result.i32_stack_top_begin_pos = result.i64_stack_top_begin_pos =
                result.f32_stack_top_begin_pos = result.f64_stack_top_begin_pos = 3;
            result.i32_stack_top_end_pos = result.i64_stack_top_end_pos =
                result.f32_stack_top_end_pos = result.f64_stack_top_end_pos = 3 + Slots;
        }
        return result;
    }

    struct bytecode
    {
        std::array<std::byte, 256> bytes{};
        std::size_t size{1}; // Every pointer and integer in this real dispatch stream is unaligned.
        template<class T> void append(T value)
        {
            CHECK(size + sizeof(value) <= bytes.size());
            std::memcpy(bytes.data() + size, &value, sizeof(value));
            size += sizeof(value);
        }
        std::byte const* end() const noexcept { return bytes.data() + size; }
    };

    template<bool Indirect, bool Tail, class... Cached>
    void one(behavior action, Cached... cached)
    {
        constexpr auto settings = option<sizeof...(Cached), Tail>();
        using pointer = std::conditional_t<Tail,
            op::uwvm_interpreter_opfunc_t<std::byte const*, std::byte*, std::byte const*, Cached...>,
            op::uwvm_interpreter_opfunc_byref_t<std::byte const*, std::byte*, std::byte const*, Cached...>>;
        auto const entry = op::translate::get_uwvmint_call_catching_fptr_from_tuple<Indirect, settings>(
            uwvm2::utils::container::tuple<std::byte const*, std::byte*, std::byte const*, Cached...>{});
        pointer next{};
        if constexpr(Tail) { next = finish<Cached...>; }
        else { next = finish_byref; }
        state s{.action = action, .indirect = Indirect, .slots = sizeof...(Cached)};
        active = &s;
        for(std::size_t i{}; i != 32; ++i) { s.frame[i] = static_cast<std::byte>(i + 1); }
        s.local_storage.fill(std::byte{0x63});
        void const* prefix_pointer = &s;
        std::memcpy(s.local_storage.data() + 1, &prefix_pointer, sizeof(prefix_pointer));
        int different_tag{};
        if(action == behavior::matching || action == behavior::nonmatching)
        {
            auto tag = std::make_shared<int>(43);
            auto referent = std::make_shared<int>(47);
            s.weak_tag = tag; s.weak_payload = referent; s.payload_identity = referent.get();
            s.accepted_tag = action == behavior::matching ? static_cast<void const*>(tag.get()) : &different_tag;
            std::array<std::byte, sizeof(payload_bits)> bits{};
            std::memcpy(bits.data(), &payload_bits, bits.size());
            std::array fields{*exn::payload_field::numeric(exn::payload_kind::f64, bits),
                              *exn::payload_field::rooted_reference(referent)};
            s.pending = exn::value::make(tag, fields);
            s.exception_identity = s.pending.get();
        }
        op::exception_call_site site{dispatch, &s};
        bytecode code;
        auto initial_ip = code.end();
        code.append(entry);
        code.append(std::size_t{7});
        code.append(std::size_t{Indirect ? 23 : 19});
        if constexpr(Indirect) { code.append(std::size_t{29}); }
        code.append(static_cast<op::exception_call_site const*>(&site));
        auto normal_ip = code.end();
        code.append(next);
        s.handler_ip = code.end();
        code.append(next);
        s.expected_ip = action == behavior::matching ? s.handler_ip : normal_ip;
        s.expected_top = action == behavior::matching ?
            s.frame.data() + 32 + sizeof(payload_bits) : s.original_top() - 8;
        auto ip = initial_ip;
        auto top = s.original_top();
        auto locals = s.locals();
        bool propagated{};
        try
        {
            pointer loaded{};
            std::memcpy(&loaded, ip, sizeof(loaded));
            loaded(ip, top, locals, cached...);
            if constexpr(!Tail)
            {
                CHECK(ip == s.expected_ip && top == s.expected_top && locals == s.locals());
                CHECK(s.resumed == 0);
                std::memcpy(&loaded, ip, sizeof(loaded));
                loaded(ip, top, locals, cached...);
                CHECK(ip == nullptr);
            }
        }
        catch(exn::guest_exception const& caught)
        {
            propagated = true;
            CHECK(action == behavior::nonmatching && s.dispatched == 1 && s.resumed == 0);
            CHECK(caught.instance().get() == s.exception_identity && caught.instance().use_count() == 1);
            CHECK(!s.weak_tag.expired() && !s.weak_payload.expired());
        }
        catch(foreign_error const& caught)
        {
            propagated = true;
            CHECK(action == behavior::foreign && caught.code == 731 && s.dispatched == 0 && s.resumed == 0);
        }
        CHECK(s.called == 1);
        CHECK(propagated == (action == behavior::nonmatching || action == behavior::foreign));
        s.check_prefix();
        if(propagated)
        {
            CHECK(ip == initial_ip && top == s.original_top() && locals == s.locals());
            CHECK(s.weak_tag.expired() && s.weak_payload.expired());
        }
        else { CHECK(s.resumed == 1); }
        if(action == behavior::matching)
        {
            // A retained catch_ref remains throwable after the first native activation has ended.
            try { exn::throw_value(s.retained); }
            catch(exn::guest_exception const& caught)
            {
                CHECK(caught.instance().get() == s.exception_identity && caught.instance().use_count() == 2);
            }
            CHECK(s.retained.use_count() == 1);
            s.retained.reset();
            CHECK(s.weak_tag.expired() && s.weak_payload.expired());
        }
        ++cases;
        active = nullptr;
    }

    template<bool Indirect>
    void matrix()
    {
        for(auto action : {behavior::normal, behavior::matching, behavior::nonmatching, behavior::foreign})
        {
            one<Indirect, false>(action);
            one<Indirect, true>(action);
            one<Indirect, true>(action, slot{.i64 = first_cached});
            one<Indirect, true>(action, slot{.i64 = first_cached}, slot{.i64 = second_cached});
        }
    }
}

int main()
{
    op::call_func = direct;
    op::call_indirect_func = indirect;
    matrix<false>();
    matrix<true>();
    std::printf("PASS protected call: %u cases, %u checks (direct/indirect, byref/tail/ring1/ring2)\n", cases, checks);
}
