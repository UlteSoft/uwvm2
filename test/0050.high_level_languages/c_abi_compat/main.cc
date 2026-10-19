#include "check.h"
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    std::vector<std::string> copied;
    for(int i = 0; i < argc; ++i) { copied.emplace_back(argv[i]); }
    assert(copied.size() == static_cast<size_t>(argc));
    for(int i = 0; i < argc; ++i) { assert(copied[i] == argv[i]); }
    return check_abi(argc, argv, "c++");
}
