// Genuine guest C++ exceptions; link only a qualified standard-Wasm-EH ABI.
// No C++ exception ABI stubs, host imports, I/O, or debugger credentials.
static_assert(sizeof(int) == 4);
static volatile int source_exception_seed = 5;
static volatile int source_exception_observed_payload;
static volatile int source_exception_observed_result;
static volatile int source_exception_cleanup_count;
static volatile int source_exception_cleanup_sum;
static volatile int source_exception_caught_count;
static volatile int source_exception_rethrow_count;

struct source_exception_error { int value; };
// Equal object layout deliberately does not mean equal C++ exception identity.
struct source_exception_wrong_error { int value; };

struct source_exception_cleanup
{
    int marker;
    explicit source_exception_cleanup(int value) noexcept : marker(value) {}
    source_exception_cleanup(source_exception_cleanup const&) = delete;
    source_exception_cleanup& operator=(source_exception_cleanup const&) = delete;
    __attribute__((noinline)) ~source_exception_cleanup() noexcept
    {
        source_exception_cleanup_count = source_exception_cleanup_count + 1;
        source_exception_cleanup_sum = source_exception_cleanup_sum + marker; // CPP_EH_CLEANUP
    }
};

__attribute__((always_inline)) static inline int source_exception_inline(int value)
{
    int adjusted = value + 3;
    source_exception_observed_result = adjusted; // CPP_EH_INLINE_IN_CATCH
    return adjusted;
}

extern "C" __attribute__((noinline, export_name("source_exception_leaf")))
int source_exception_leaf(int value)
{
    source_exception_cleanup cleanup(1);
    int payload = value + 2;
    source_exception_observed_payload = payload; // CPP_EH_LEAF_ENTRY
    throw source_exception_error{payload}; // CPP_EH_THROW
}

extern "C" __attribute__((noinline, export_name("source_exception_rethrow")))
int source_exception_rethrow(int value)
{
    source_exception_cleanup function_cleanup(2);
    try
    {
        source_exception_cleanup try_cleanup(4);
        source_exception_observed_payload = value; // CPP_EH_INNER_TRY
        source_exception_leaf(value); // CPP_EH_INNER_CALL
        __builtin_trap();
    }
    catch(source_exception_error const& error)
    {
        // [safe] error is borrowed only while this real guest catch owns the
        // active C++ exception; this grants no host debugger memory authority.
        source_exception_caught_count = source_exception_caught_count + 1;
        source_exception_observed_payload = error.value; // CPP_EH_INNER_CATCH
        if(source_exception_cleanup_count != 2 || source_exception_cleanup_sum != 5) __builtin_trap();
        source_exception_cleanup catch_cleanup(8);
        source_exception_rethrow_count = source_exception_rethrow_count + 1;
        throw; // CPP_EH_RETHROW
    }
}

extern "C" __attribute__((noinline, export_name("source_exception_outer")))
int source_exception_outer(int value)
{
    source_exception_cleanup function_cleanup(16);
    int result = 0;
    try
    {
        source_exception_cleanup try_cleanup(32);
        source_exception_observed_payload = value; // CPP_EH_OUTER_TRY
        result = source_exception_rethrow(value); // CPP_EH_OUTER_CALL
        __builtin_trap();
    }
    catch(source_exception_wrong_error const&)
    {
        __builtin_trap(); // CPP_EH_WRONG_TYPE_UNREACHABLE
    }
    catch(source_exception_error const& error)
    {
        // [safe] the typed reference stays within its genuine catch lifetime.
        source_exception_cleanup catch_cleanup(64);
        source_exception_caught_count = source_exception_caught_count + 1;
        if(source_exception_cleanup_count != 5 || source_exception_cleanup_sum != 47) __builtin_trap();
        result = source_exception_inline(error.value); // CPP_EH_OUTER_CATCH
    }
    if(source_exception_cleanup_count != 6 || source_exception_cleanup_sum != 111) __builtin_trap();
    source_exception_observed_result = result; // CPP_EH_AFTER_CATCH
    return result; // CPP_EH_OUTER_RETURN
}

extern "C" __attribute__((export_name("_start"))) void _start()
{
    int result = source_exception_outer(source_exception_seed);
    if(result != 10 || source_exception_observed_result != 10 || source_exception_observed_payload != 7) __builtin_trap();
    if(source_exception_caught_count != 2 || source_exception_rethrow_count != 1) __builtin_trap();
    if(source_exception_cleanup_count != 7 || source_exception_cleanup_sum != 127) __builtin_trap();
}
