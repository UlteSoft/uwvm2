// Concurrent callers exercise the production verifier, key-math memo and
// protected-state locking. This does not compile or execute a guest backend.
#include <uwvm2/utils/security/wsea.h>
#include <barrier>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

struct input_case
{
    std::string policy, path, forged_path, image, forged;
};

struct outcome
{
    bool passed{};
    char const* positive_code{};
    char const* negative_code{};
};

static std::string read_file(std::string const& path)
{
    std::ifstream stream(path, std::ios::binary);
    if(!stream) { throw std::runtime_error("test input open failed"); }
    return {std::istreambuf_iterator<char>{stream}, {}};
}

int main(int argc, char** argv)
{
    if(argc != 2) { return 64; }
    try
    {
        std::vector<input_case> cases;
        std::ifstream requests(argv[1]);
        std::string line;
        while(std::getline(requests, line))
        {
            auto first{line.find('\t')}, second{first == line.npos ? first : line.find('\t', first+1)};
            if(first == line.npos || second == line.npos || line.find('\t', second+1) != line.npos) { return 64; }
            input_case item{line.substr(0,first), line.substr(first+1,second-first-1), line.substr(second+1), {}, {}};
            item.image = read_file(item.path);
            item.forged = read_file(item.forged_path);
            cases.push_back(std::move(item));
        }
        if(!requests.eof() || cases.size() != 17) { return 64; }
        constexpr std::size_t workers{8}, rounds{4};
        auto const per_worker{1 + rounds*cases.size()};
        std::vector<outcome> outcomes(workers*per_worker);
        std::barrier start{static_cast<std::ptrdiff_t>(workers)};
        std::vector<std::thread> threads;
        threads.reserve(workers);
        for(std::size_t worker{}; worker != workers; ++worker)
        {
            threads.emplace_back([&, worker]
            {
                start.arrive_and_wait();
                for(std::size_t iteration{}; iteration != per_worker; ++iteration)
                {
                    // All callers initially enter the same cold RSA8192 key;
                    // later calls rotate all profiles through shared key math.
                    auto const index{iteration == 0 ? cases.size()-1 : (iteration-1+worker)%cases.size()};
                    auto const& item{cases[index]};
                    namespace wsea = uwvm2::utils::security::wsea;
                    auto bytes{reinterpret_cast<std::byte const*>(item.image.data())};
                    auto bad_bytes{reinterpret_cast<std::byte const*>(item.forged.data())};
                    auto positive{wsea::verify_module(item.policy, item.path, bytes, item.image.size())};
                    auto negative{wsea::verify_module(item.policy, item.forged_path, bad_bytes, item.forged.size())};
                    std::string copy{item.image};
                    bool const passed{positive && positive.authorization &&
                        positive.authorization->capabilities() == 3 &&
                        positive.authorization->matches(bytes, item.image.size()) &&
                        !positive.authorization->matches(reinterpret_cast<std::byte const*>(copy.data()), copy.size()) &&
                        !positive.authorization->matches(bad_bytes, item.forged.size()) &&
                        !negative && !negative.authorization && negative.code && std::string_view{negative.code} == "SIG_003"};
                    outcomes[worker*per_worker+iteration] = {passed, positive.code, negative.code};
                }
            });
        }
        for(auto& thread : threads) { thread.join(); }
        namespace wsea = uwvm2::utils::security::wsea;
        std::shared_ptr<wsea::detail::policy const> policy;
        auto loaded{wsea::detail::load_policy(cases.back().policy, policy)};
        wsea::detail::embedded_evidence evidence;
        wsea::detail::manifest manifest;
        if(!loaded || !wsea::detail::extract_embedded(cases.back().image, evidence) ||
           !wsea::detail::decode(evidence.cose, manifest)) { return 70; }
        auto now{std::time(nullptr)};
        bool controls[4]{};
        controls[0] = static_cast<bool>(wsea::detail::commit_state(*policy, manifest, now-2, static_cast<std::uint64_t>(now)+60));
        auto state_path{policy->state_path + "/state"};
        auto unchanged{read_file(state_path)};
        auto expired{wsea::detail::commit_state(*policy, manifest, now-2, static_cast<std::uint64_t>(now))};
        controls[1] = !expired && std::string_view{expired.code} == "POL_004" && read_file(state_path) == unchanged;
        auto reversed{wsea::detail::commit_state(*policy, manifest, now+60, static_cast<std::uint64_t>(now)+120)};
        controls[2] = !reversed && std::string_view{reversed.code} == "POL_004" && read_file(state_path) == unchanged;
        // A dedicated final control advances only the remembered wall-clock
        // floor. Never restore it or execute another authorization afterward.
        auto first_line{unchanged.find('\n')}, first_space{unchanged.find(' ',first_line+1)}, last_space{unchanged.find(' ',first_space+1)};
        if(first_line == unchanged.npos || first_space == unchanged.npos || last_space == unchanged.npos) { return 70; }
        auto future{unchanged.substr(0,first_space+1) + std::to_string(now+3600) + unchanged.substr(last_space)};
        { std::ofstream stream(state_path, std::ios::binary | std::ios::trunc); stream << future; if(!stream) { return 70; } }
        auto rollback{wsea::detail::commit_state(*policy, manifest, now, static_cast<std::uint64_t>(now)+60)};
        controls[3] = !rollback && std::string_view{rollback.code} == "POL_004" && read_file(state_path) == future;
        std::size_t failures{};
        std::printf("{\"threads\":%zu,\"rounds\":%zu,\"cases\":%zu,\"records\":[", workers, rounds, cases.size());
        for(std::size_t index{}; index != outcomes.size(); ++index)
        {
            auto const& value{outcomes[index]};
            if(!value.passed) { ++failures; }
            std::printf("%s{\"worker\":%zu,\"iteration\":%zu,\"passed\":%s,\"positive_code\":\"%s\",\"negative_code\":\"%s\"}",
                index ? "," : "", index/per_worker, index%per_worker, value.passed ? "true" : "false",
                value.positive_code ? value.positive_code : "ALLOW", value.negative_code ? value.negative_code : "ALLOW");
        }
        std::printf("],\"controls\":[");
        for(std::size_t index{}; index != 4; ++index)
        {
            if(!controls[index]) { ++failures; }
            std::printf("%s{\"index\":%zu,\"passed\":%s}", index ? "," : "", index, controls[index] ? "true" : "false");
        }
        std::printf("],\"status\":\"%s\"}\n", failures ? "FAIL" : "PASS");
        return failures ? 2 : 0;
    }
    catch(...) { std::fputs("HARNESS_ERROR concurrent test exception\n", stderr); return 70; }
}
