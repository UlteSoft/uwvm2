// This runner exercises the production verifier without a replacement crypto implementation.
#include <uwvm2/utils/security/wsea.h>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    namespace wsea = uwvm2::utils::security::wsea;
    if(argc < 3 || argc > 4) { return 64; }
    auto configured = wsea::configure_policy(argv[1]);
    if(configured.code != nullptr)
    {
        std::printf("DENY %s %s\n", configured.code, configured.message == nullptr ? "" : configured.message);
        return 2;
    }
    std::ifstream source(argv[2], std::ios::binary);
    if(!source) { return 66; }
    std::vector<char> bytes{std::istreambuf_iterator<char>(source), {}};
    if(argc == 4 && std::string(argv[3]) == "replace")
    {
        std::ofstream replaced(argv[2], std::ios::binary | std::ios::trunc);
        replaced << "untrusted replacement after snapshot";
    }
    auto checked = wsea::verify_module(argv[2], reinterpret_cast<std::byte const*>(bytes.data()), bytes.size());
    if(checked.code != nullptr)
    {
        std::printf("DENY %s %s\n", checked.code, checked.message == nullptr ? "" : checked.message);
        return 2;
    }
    if(!checked.authorization || !checked.authorization->matches(reinterpret_cast<std::byte const*>(bytes.data()), bytes.size()))
    {
        std::puts("FAIL authorization does not match verified byte buffer");
        return 3;
    }
    if(argc == 4 && std::string(argv[3]) == "mutate")
    {
        if(bytes.empty()) { return 3; }
        bytes.back() ^= 1;
        if(checked.authorization->matches(reinterpret_cast<std::byte const*>(bytes.data()), bytes.size()))
        {
            std::puts("FAIL mutated byte buffer retained authorization");
            return 3;
        }
        std::puts("ALLOW mutation rejected by authorization seal");
        return 0;
    }
    if(argc == 4 && std::string(argv[3]) == "change-policy")
    {
        std::ofstream changed(argv[1], std::ios::app);
        changed << "untrusted_update=1\n";
        changed.close();
        if(checked.authorization->matches(reinterpret_cast<std::byte const*>(bytes.data()), bytes.size()))
        {
            std::puts("FAIL changed policy retained old authorization");
            return 3;
        }
        std::puts("ALLOW changed policy rejected by authorization seal");
        return 0;
    }
    if(argc == 4 && std::string(argv[3]).starts_with("replace-crl="))
    {
        std::ifstream replacement(std::string(argv[3]).substr(12), std::ios::binary);
        std::ofstream changed(std::string(argv[2]) + ".crl.pem", std::ios::binary | std::ios::trunc);
        changed << replacement.rdbuf();
        changed.close();
        if(checked.authorization->matches(reinterpret_cast<std::byte const*>(bytes.data()), bytes.size()))
        {
            std::puts("FAIL changed revocation evidence retained old authorization");
            return 3;
        }
        std::puts("ALLOW changed revocation evidence rejected by authorization seal");
        return 0;
    }
    if(argc == 4 && std::string(argv[3]).starts_with("replace-signature="))
    {
        std::ifstream replacement(std::string(argv[3]).substr(18), std::ios::binary);
        std::ofstream changed(std::string(argv[2]) + ".sig.cose", std::ios::binary | std::ios::trunc);
        changed << replacement.rdbuf();
        changed.close();
        if(checked.authorization->matches(reinterpret_cast<std::byte const*>(bytes.data()), bytes.size()))
        {
            std::puts("FAIL a different signed manifest retained old authorization");
            return 3;
        }
        std::puts("ALLOW different signed manifest rejected by authorization seal");
        return 0;
    }
    std::puts("ALLOW");
}
