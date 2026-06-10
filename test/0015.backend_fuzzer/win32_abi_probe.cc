#if defined(UWVM_TEST_FREESTANDING_C_TYPES)
using size_t = decltype(sizeof(0));
using uintptr_t = __UINTPTR_TYPE__;
using uint64_t = unsigned long long;
using uint_least32_t = unsigned int;
extern "C" void* memcpy(void*, void const*, size_t);
# define offsetof(type, member) __builtin_offsetof(type, member)
#else
# include <stddef.h>
# include <stdint.h>
# include <string.h>
#endif

#if defined(_WIN32) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) && \
    (defined(__GNUC__) || defined(__clang__))
# define UWVM_TEST_WASM_ABI __attribute__((__sysv_abi__))
#elif defined(__i386__) || defined(_M_IX86)
# define UWVM_TEST_WASM_ABI __attribute__((__fastcall__))
#else
# define UWVM_TEST_WASM_ABI
#endif

namespace
{
    using raw_entry_fn = void(UWVM_TEST_WASM_ABI*)(uintptr_t, uintptr_t, size_t, uintptr_t, size_t);
    using typed_entry_fn = uint64_t(UWVM_TEST_WASM_ABI*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
    using host_bridge_fn = void (*)(uintptr_t, uint_least32_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t) noexcept;
    using callback_fn = void(UWVM_TEST_WASM_ABI*)(size_t, size_t, unsigned char**) noexcept;

    struct raw_call_target
    {
        uintptr_t entry_address{};
        uintptr_t context_address{};
        uint_least32_t encoded_type_id{};
        uintptr_t typed_entry_address{};
    };

    static_assert(__is_trivially_copyable(raw_call_target));
    static_assert(offsetof(raw_call_target, entry_address) == 0u);
    static_assert(offsetof(raw_call_target, context_address) == sizeof(uintptr_t));
    static_assert(offsetof(raw_call_target, typed_entry_address) % alignof(uintptr_t) == 0u);

    struct raw_context
    {
        uint64_t cookie{};
        typed_entry_fn typed_entry{};
        raw_entry_fn reentry{};
    };

    inline volatile uint64_t g_error_code{};
    inline volatile uint64_t g_bridge_sink{};

    constexpr uint64_t rotl(uint64_t v, unsigned s) noexcept
    { return static_cast<uint64_t>((v << (s & 63u)) | (v >> ((64u - s) & 63u))); }

    constexpr uintptr_t ptr_cookie(uint64_t value) noexcept
    { return static_cast<uintptr_t>(value); }

    inline constexpr uintptr_t bridge_module_cookie{ptr_cookie(0x1020304050607080ull)};
    inline constexpr uintptr_t bridge_result_cookie{ptr_cookie(0x1122334455667788ull)};
    inline constexpr uintptr_t bridge_param_cookie{ptr_cookie(0x8877665544332211ull)};

    constexpr uint64_t typed_expected(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) noexcept
    {
        return static_cast<uint64_t>((a0 + rotl(a1, 7u)) ^ (a2 * 0x100000001b3ull) ^ (a3 + 0x9e3779b97f4a7c15ull) ^ rotl(a4, 17u) ^
                                     (a5 * 33u) ^ (a6 + a7));
    }

    void fail(uint64_t code) noexcept
    { g_error_code = g_error_code == 0u ? code : static_cast<uint64_t>(g_error_code * 131u + code); }

    void expect(bool condition, uint64_t code) noexcept
    {
        if(!condition) [[unlikely]] { fail(code); }
    }

    uint64_t UWVM_TEST_WASM_ABI typed_entry_probe(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) noexcept
    {
        return typed_expected(a0, a1, a2, a3, a4, a5, a6, a7);
    }

    void UWVM_TEST_WASM_ABI raw_entry_probe(uintptr_t context_address, uintptr_t result_buffer_address, size_t result_bytes, uintptr_t param_buffer_address, size_t param_bytes) noexcept
    {
        auto* const context{reinterpret_cast<raw_context const*>(context_address)};
        auto* const result{reinterpret_cast<uint64_t*>(result_buffer_address)};
        auto* const params{reinterpret_cast<uint64_t const*>(param_buffer_address)};
        expect(context != nullptr, 1001u);
        expect(result != nullptr && result_bytes == sizeof(uint64_t), 1002u);
        expect(params != nullptr && param_bytes == sizeof(uint64_t) * 8u, 1003u);
        if(context == nullptr || result == nullptr || params == nullptr) { return; }
        expect(context->cookie == 0x7a6b5c4d3e2f1908ull, 1004u);
        auto const value{context->typed_entry(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7])};
        memcpy(result, &value, sizeof(value));
    }

    void host_bridge_probe(uintptr_t module_address,
                           uint_least32_t function_index,
                           uintptr_t result_buffer_address,
                           uintptr_t result_bytes,
                           uintptr_t param_buffer_address,
                           uintptr_t param_bytes) noexcept
    {
        auto const mixed{module_address ^ (static_cast<uint64_t>(function_index) << 32u) ^ result_buffer_address ^ result_bytes ^
                         param_buffer_address ^ param_bytes};
        g_bridge_sink ^= mixed;
        expect(module_address == bridge_module_cookie, 2001u);
        expect(function_index == 0x89abu, 2002u);
        expect(result_buffer_address == bridge_result_cookie, 2003u);
        expect(result_bytes == 8u, 2004u);
        expect(param_buffer_address == bridge_param_cookie, 2005u);
        expect(param_bytes == 64u, 2006u);
    }

    void UWVM_TEST_WASM_ABI wasm_calls_host_bridge(host_bridge_fn bridge) noexcept
    { bridge(bridge_module_cookie, 0x89abu, bridge_result_cookie, 8u, bridge_param_cookie, 64u); }

    void host_calls_typed_entry() noexcept
    {
        typed_entry_fn const typed{typed_entry_probe};
        auto const value{typed(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u)};
        expect(value == typed_expected(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u), 3001u);
    }

    void host_calls_raw_entry() noexcept
    {
        raw_context context{0x7a6b5c4d3e2f1908ull, typed_entry_probe, raw_entry_probe};
        uint64_t params[8]{0x10u, 0x21u, 0x32u, 0x43u, 0x54u, 0x65u, 0x76u, 0x87u};
        uint64_t result{};
        raw_entry_fn const raw{raw_entry_probe};
        raw(reinterpret_cast<uintptr_t>(&context),
            reinterpret_cast<uintptr_t>(&result),
            sizeof(result),
            reinterpret_cast<uintptr_t>(params),
            sizeof(params));
        expect(result == typed_expected(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7]), 4001u);
    }

    uint64_t UWVM_TEST_WASM_ABI table_dispatch_probe(raw_call_target const* target, uint64_t const* params) noexcept
    {
        expect(target != nullptr, 5001u);
        expect(params != nullptr, 5002u);
        expect(target != nullptr && target->encoded_type_id == 0x513u, 5003u);
        if(target == nullptr || params == nullptr) { return 0u; }

        if(target->typed_entry_address != 0u)
        {
            auto const typed{reinterpret_cast<typed_entry_fn>(target->typed_entry_address)};
            return typed(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7]);
        }

        uint64_t result{};
        auto const raw{reinterpret_cast<raw_entry_fn>(target->entry_address)};
        raw(target->context_address, reinterpret_cast<uintptr_t>(&result), sizeof(result), reinterpret_cast<uintptr_t>(params), sizeof(uint64_t) * 8u);
        return result;
    }

    void host_calls_table_dispatch() noexcept
    {
        raw_context context{0x7a6b5c4d3e2f1908ull, typed_entry_probe, raw_entry_probe};
        uint64_t params[8]{11u, 13u, 17u, 19u, 23u, 29u, 31u, 37u};
        auto const expected{typed_expected(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7])};

        raw_call_target raw_target{reinterpret_cast<uintptr_t>(raw_entry_probe),
                                   reinterpret_cast<uintptr_t>(&context),
                                   0x513u,
                                   0u};
        raw_call_target typed_target{reinterpret_cast<uintptr_t>(raw_entry_probe),
                                     reinterpret_cast<uintptr_t>(&context),
                                     0x513u,
                                     reinterpret_cast<uintptr_t>(typed_entry_probe)};

        expect(table_dispatch_probe(&raw_target, params) == expected, 5004u);
        expect(table_dispatch_probe(&typed_target, params) == expected, 5005u);
    }

    void UWVM_TEST_WASM_ABI reentry_driver(raw_entry_fn reentry, raw_context const* context, uint64_t const* params, uint64_t* result) noexcept
    {
        reentry(reinterpret_cast<uintptr_t>(context),
                reinterpret_cast<uintptr_t>(result),
                sizeof(*result),
                reinterpret_cast<uintptr_t>(params),
                sizeof(uint64_t) * 8u);
    }

    void host_calls_reentry_driver() noexcept
    {
        raw_context context{0x7a6b5c4d3e2f1908ull, typed_entry_probe, raw_entry_probe};
        uint64_t params[8]{101u, 103u, 107u, 109u, 113u, 127u, 131u, 137u};
        uint64_t result{};
        reentry_driver(raw_entry_probe, &context, params, &result);
        expect(result == typed_expected(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7]), 6001u);
    }

    void UWVM_TEST_WASM_ABI interpreter_callback_probe(size_t module_id, size_t function_index, unsigned char** stack_top) noexcept
    {
        expect(module_id == 7u, 7001u);
        expect(function_index == 42u, 7002u);
        expect(stack_top != nullptr && *stack_top != nullptr, 7003u);
        if(stack_top != nullptr && *stack_top != nullptr) { *stack_top += 16; }
    }

    void UWVM_TEST_WASM_ABI opfunc_driver(callback_fn callback, unsigned char** stack_top) noexcept
    { callback(7u, 42u, stack_top); }

    void host_calls_interpreter_callback_driver() noexcept
    {
        unsigned char stack[64]{};
        auto* top{stack};
        opfunc_driver(interpreter_callback_probe, &top);
        expect(top == stack + 16, 7004u);
    }

    void run_all() noexcept
    {
        host_calls_typed_entry();
        host_calls_raw_entry();
        wasm_calls_host_bridge(host_bridge_probe);
        host_calls_table_dispatch();
        host_calls_reentry_driver();
        host_calls_interpreter_callback_driver();
    }
}  // namespace

int main()
{
    run_all();
    return g_error_code == 0u ? 0 : static_cast<int>((g_error_code & 0x7fu) + 1u);
}

#undef UWVM_TEST_WASM_ABI
