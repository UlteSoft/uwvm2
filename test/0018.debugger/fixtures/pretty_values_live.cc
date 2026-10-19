// Real C++23 producer v1. Compile unchanged with -std=c++23 -O0 -g.
// The separate compiler/-MD receipt, not this source, qualifies the target ABI.
#include <chrono>
#include <cstdlib>
#include <deque>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <stack>
#include <utility>
#include <variant>
#include <vector>
#if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L
# error "This real producer requires the deployed C++23 std::expected implementation"
#endif
extern "C" __attribute__((used)) char const pretty_values_fixture_identity[]=
    "uwvm-dbg-pretty-values-live-v1;c++23;real-standard-library";
extern "C" __attribute__((used)) char const pretty_values_compiler_identity[]=
#if defined(__clang__)
    "clang " __clang_version__;
#else
    "gcc " __VERSION__;
#endif
struct PrettyThrowOnMove
{
    int value;
    explicit PrettyThrowOnMove(int n) noexcept:value(n) {}
    PrettyThrowOnMove(PrettyThrowOnMove const&)=delete;
    PrettyThrowOnMove& operator=(PrettyThrowOnMove const&)=delete;
    [[noreturn]] static void fail_move()
    {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        throw 73;
#else
        ::std::abort();
#endif
    }
    PrettyThrowOnMove(PrettyThrowOnMove&&) { fail_move(); }
    PrettyThrowOnMove& operator=(PrettyThrowOnMove&&) { fail_move(); }
};
using PrettyNumberList=::std::vector<int>;
volatile int pretty_values_result;
extern "C" __attribute__((noinline)) int pretty_values_probe()
{
    ::std::expected<int,int> expected_value{23};
    ::std::expected<int,int> expected_error{::std::unexpected{29}};
    ::std::expected<void,int> expected_void_value{};
    ::std::expected<void,int> expected_void_error{::std::unexpected{31}};
    ::std::optional<int> present{37},absent{};
    ::std::variant<int,bool> active{::std::in_place_index<1>,true};
    ::std::variant<int,PrettyThrowOnMove> valueless{::std::in_place_index<0>,0};
    int valueless_exceptions{},valueless_caught{};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    valueless_exceptions=1;
    ::std::variant<int,PrettyThrowOnMove> throwing_source{::std::in_place_index<1>,73};
    try { valueless=::std::move(throwing_source); }
    catch(int code) { valueless_caught=code==73; }
#endif
    auto shared{::std::make_shared<int>(41)};
    ::std::weak_ptr<int> weak_live{shared};
    ::std::shared_ptr<void> alias_void{shared,static_cast<void*>(shared.get())};
    ::std::weak_ptr<int> weak_expired{};
    { auto expired_owner{::std::make_shared<int>(43)};weak_expired=expired_owner; }
    ::std::shared_ptr<int[]> array_owned{new int[3]{51,52,53}};
    ::std::chrono::milliseconds ticks{1234};
    ::std::chrono::time_point<::std::chrono::steady_clock,::std::chrono::milliseconds> point{ticks};
    ::std::stack<int,::std::vector<int>> stack_vector{::std::vector<int>{61,62,63}};
    ::std::stack<int,::std::deque<int>> stack_deque{::std::deque<int>{64,65,66}};
    ::std::queue<int,::std::deque<int>> queue_deque{::std::deque<int>{71,72,73}};
    ::std::priority_queue<int,::std::vector<int>> priority_vector{::std::less<int>{},::std::vector<int>{81,82,83}};
    ::std::priority_queue<int,::std::deque<int>> priority_deque{::std::less<int>{},::std::deque<int>{84,85,86}};
    PrettyNumberList typedef_values{91,92,93};
    asm volatile("" : : : "memory");
    pretty_values_result=*expected_value+*present+*shared+static_cast<int>(ticks.count()); // PRETTY_VALUES_LIVE_STOP
    bool const values_ok{*expected_value==23 && !expected_error && expected_error.error()==29 &&
        expected_void_value.has_value() && !expected_void_error && expected_void_error.error()==31 &&
        present==37 && !absent && active.index()==1u && ::std::get<1>(active)};
    bool const ownership_ok{*shared==41 && shared.use_count()==2 && weak_live.use_count()==2 &&
        !weak_live.expired() && weak_expired.use_count()==0 && weak_expired.expired() &&
        alias_void.get()==shared.get() && alias_void.use_count()==2 && array_owned[2]==53};
    bool const containers_ok{ticks.count()==1234 && point.time_since_epoch().count()==1234 &&
        stack_vector.size()==3u && stack_vector.top()==63 && stack_deque.top()==66 &&
        queue_deque.front()==71 && queue_deque.back()==73 && priority_vector.top()==83 &&
        priority_deque.top()==86 && typedef_values[0]==91 && typedef_values[2]==93};
    bool const exception_ok{!valueless_exceptions || (valueless_caught && valueless.valueless_by_exception())};
    return values_ok && ownership_ok && containers_ok && exception_ok && pretty_values_result==1335 ? 0 : 1;
}
int main() { return pretty_values_probe(); }
