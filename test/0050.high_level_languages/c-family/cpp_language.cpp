#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

class Counter {
public:
    virtual int add(int amount) = 0;
    virtual ~Counter() = default;
};

class CheckedCounter final : public Counter {
    int value_ = 40;
public:
    int add(int amount) override { return value_ += amount; }
};

extern "C" __attribute__((noinline)) int debug_value() {
    int value = 40;
    value += 2; // DEBUG_BEFORE
    return value; // DEBUG_AFTER
}

int main() {
    CheckedCounter object;
    Counter &base = object;
    std::array<int, 10> numbers{};
    for (std::size_t i = 0; i < numbers.size(); ++i)
        numbers[i] = static_cast<int>(i + 1);
    std::vector<int> copy(numbers.begin(), numbers.end());
    int sum = 0;
    for (int number : copy) sum += number;
    std::string text = "\xcf\x80\xe4\xbd\xa0\xe5\xa5\xbd\xf0\x9f\x99\x82";
    std::uint32_t checksum = 7;
    for (std::uint32_t i = 0; i < 100; ++i)
        checksum = (checksum * 33u + i) ^ 0x55aau;
    int classes = base.add(2);
    int value = debug_value();
    if (classes != 42 || sum != 55 || text.size() != 12 || value != 42)
        return 2;
    std::printf("cpp foundation classes=%d arrays=%d utf8=%zu value=%d checksum=%u\n",
                classes, sum, text.size(), value, static_cast<unsigned>(checksum));
    return 0;
}
