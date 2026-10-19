// Compiler-produced standard-library DWARF, used by the live CLI/DAP tests.
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

volatile int pretty_result;
extern "C" __attribute__((noinline)) int pretty_container_probe(int seed)
{
    std::vector<int> values{seed,seed+1,seed+2,seed+3,seed+4,seed+5,seed+6,seed+7};
    std::vector<std::vector<int>> nested{{seed,seed+1},{seed+2,seed+3}};
    std::string short_text{"small"};
    std::string long_text{"this is a string longer than the short string buffer"};
    std::string_view view{short_text};
    std::array<int,3> fixed{seed,seed+1,seed+2};
    std::span<int> span{values};
    std::optional<int> present{seed};
    std::optional<int> absent{};
    std::variant<int,bool> variant{true};
    std::unique_ptr<int> unique{new int{seed}};
    std::shared_ptr<int> shared{std::make_shared<int>(seed+1)};
    std::weak_ptr<int> weak{shared};
    std::pair<int,bool> pair{seed,true};
    std::tuple<int,bool,int> tuple{seed,true,seed+2};
    pretty_result = values[0]+static_cast<int>(short_text.size())+*present; // PRETTY_CONTAINERS_STOP
    return pretty_result+nested[1][1]+static_cast<int>(long_text.size())+view[0]+fixed[2]+span[1]+
        (absent ? 1 : 0)+std::get<bool>(variant)+*unique+*shared+(weak.expired() ? 1 : 0)+pair.first+std::get<2>(tuple);
}

int main() { return pretty_container_probe(7) == 0; }
