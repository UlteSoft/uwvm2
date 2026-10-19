// Real compiler-produced C++20 coroutine fixture, version 1.
//
// Intended producers (record exact --version, target, flags and file hash):
//   WASI SDK 24 / Clang 18: wasm32-wasi, -std=c++20 -O0 -g -gdwarf-4/5
//   Native GCC 15:         native target, -std=c++20 -O0 -g -gdwarf-4/5
// Do not compile or execute on the local host. Use the authorized SSH Linux
// 64 GiB cgroup; native GCC qualification is distinct from the Wasm guest.
//
// Break at pretty_coroutine_ready. Its `typed`, `erased` and `phase` arguments
// are real DWARF variables. Phase 0 is initial suspension, phases 1/2 are
// yields, phase 3 is final suspension. Both handles intentionally contain the
// same erased frame address: printing them must not invent a concrete frame or
// promise type. Current safe output is a guest frame offset + ABI unavailable.
//
// For actual compiler-frame qualification, also break at the marked source
// lines inside pretty_coroutine_sequence and query __coro_frame / __promise.
// Inspect their concrete owning DIE/type/location before attempting a source
// query. Clang commonly emits an artificial *.coro_frame_ty with __resume_fn,
// __destroy_fn and __coro_index. GCC's representation and artificial-variable
// availability differ. If those real variables cannot be selected/materialized
// by the existing Wasm DWARF evaluator, retain ABI unavailable; synthetic schema
// tests do not establish real producer or lifecycle qualification. Neither
// compiler's private suspension index alone identifies a source line here.
#include <coroutine>

#if defined(__GNUC__) || defined(__clang__)
# define PRETTY_COROUTINE_NOINLINE __attribute__((noinline))
# define PRETTY_COROUTINE_USED __attribute__((used))
#else
# define PRETTY_COROUTINE_NOINLINE
# define PRETTY_COROUTINE_USED
#endif

extern "C" PRETTY_COROUTINE_USED char const pretty_coroutine_fixture_identity[] =
    "uwvm-dbg-pretty-coroutine-v1;language=c++20;stops=initial,yield1,yield2,final,empty";
extern "C" PRETTY_COROUTINE_USED char const pretty_coroutine_compiler_identity[] =
#if defined(__clang__)
    "clang " __clang_version__;
#elif defined(__GNUC__)
    "gcc " __VERSION__;
#else
    "unknown compiler; record external compiler version";
#endif

struct pretty_generator
{
    struct promise_type;
    using handle_type = ::std::coroutine_handle<promise_type>;
    struct promise_type
    {
        int current_value{};
        pretty_generator get_return_object() noexcept
        { return pretty_generator{handle_type::from_promise(*this)}; }
        ::std::suspend_always initial_suspend() const noexcept { return {}; }
        ::std::suspend_always final_suspend() const noexcept { return {}; }
        ::std::suspend_always yield_value(int value) noexcept
        { current_value=value;return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept { __builtin_trap(); }
    };
    handle_type handle{};
    explicit pretty_generator(handle_type value) noexcept : handle(value) {}
    pretty_generator(pretty_generator const&) = delete;
    pretty_generator& operator=(pretty_generator const&) = delete;
    pretty_generator(pretty_generator&& other) noexcept : handle(other.handle) { other.handle=nullptr; }
    ~pretty_generator() { if(handle) { handle.destroy(); } }
};

static volatile unsigned pretty_coroutine_observed_phase{};
static volatile int pretty_coroutine_observed_value{};
static void* volatile pretty_coroutine_observed_address{};

extern "C" PRETTY_COROUTINE_NOINLINE void pretty_coroutine_ready(
    ::std::coroutine_handle<pretty_generator::promise_type> typed,
    ::std::coroutine_handle<> erased, unsigned phase) noexcept
{
    // Marker: inspect real argument objects while the generator is suspended.
    pretty_coroutine_observed_phase=phase;
    pretty_coroutine_observed_address=erased.address();
    pretty_coroutine_observed_value=typed.address()==erased.address() ? static_cast<int>(phase) : -1;
}
extern "C" PRETTY_COROUTINE_NOINLINE void pretty_coroutine_empty_ready(::std::coroutine_handle<> erased) noexcept
{
    // Marker: a real zero-address erased handle; no frame exists to read.
    pretty_coroutine_observed_address=erased.address();
}
extern "C" PRETTY_COROUTINE_NOINLINE void pretty_coroutine_body_ready(int previous,int current) noexcept
{
    // For a stop here, select the actual coroutine caller frame before querying
    // its artificial compiler variables. This helper's own frame has none.
    pretty_coroutine_observed_value=previous+current;
}

PRETTY_COROUTINE_NOINLINE pretty_generator pretty_coroutine_sequence(int seed)
{
    int previous{seed};
    int current{seed+1};
    pretty_coroutine_body_ready(previous,current); // real source stop before yield 1
    co_yield previous; // concrete coroutine-scope DWARF selection candidate
    previous=current;
    ++current;
    pretty_coroutine_body_ready(previous,current); // real source stop before yield 2
    co_yield previous; // concrete coroutine-scope DWARF selection candidate
    co_return;
}

int main()
{
    auto generator{pretty_coroutine_sequence(40)};
    auto typed{generator.handle};
    auto erased{::std::coroutine_handle<>::from_address(typed.address())};
    pretty_coroutine_ready(typed,erased,0u);
    if(typed.done()) { return 1; }
    typed.resume();
    pretty_coroutine_ready(typed,erased,1u);
    if(typed.done() || typed.promise().current_value!=40) { return 2; }
    typed.resume();
    pretty_coroutine_ready(typed,erased,2u);
    if(typed.done() || typed.promise().current_value!=41) { return 3; }
    typed.resume();
    pretty_coroutine_ready(typed,erased,3u);
    if(!typed.done()) { return 4; }
    pretty_coroutine_empty_ready({});
    // The owner destroys the frame after the last marker. No stale handle is
    // inspected after destruction, and zeroed bytes never prove destruction.
    return pretty_coroutine_fixture_identity[0]=='u' && pretty_coroutine_compiler_identity[0]!='\0' ? 0 : 5;
}
