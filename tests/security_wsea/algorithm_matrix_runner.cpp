// Production WSEA algorithm-matrix runner; no replacement cryptography.
#include <uwvm2/utils/security/wsea.h>
#include <bit>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <iostream>
#include <string>
#include <vector>

static char const* target_arch() noexcept
{
#if defined(__aarch64__)
    return "aarch64";
#elif defined(__x86_64__)
    return "x86_64";
#elif defined(__i386__)
    return "i686";
#elif defined(__arm__)
    return "arm";
#elif defined(__powerpc64__)
    return "ppc64";
#elif defined(__mips__) && (defined(__mips64) || defined(__mips64__))
    return "mips64";
#elif defined(__loongarch64) || defined(__loongarch64__) || (defined(__loongarch__) && __loongarch_grlen == 64)
    return "loongarch64";
#else
    return "unknown";
#endif
}
int main(int argc, char** argv)
{
    namespace wsea = uwvm2::utils::security::wsea;
    if(argc==2 && std::string(argv[1])=="--batch")
    {
        // Reuse mathematical-key validation in one process while every case
        // still performs the complete production authorization and receipt.
        // Each line has protected policy, module path and explicit identity.
        std::string line;
        while(std::getline(std::cin,line))
        {
            auto first{line.find('\t')}, second{first==std::string::npos ? first : line.find('\t',first+1)};
            if(first==std::string::npos || second==std::string::npos) { return 64; }
            auto path{line.substr(first+1,second-first-1)};
            try
            {
                std::shared_ptr<wsea::detail::policy const> policy;
                auto checked{wsea::detail::load_policy(line.substr(0,first),policy)};
                if(!checked) { std::printf("2\tDENY %s %s\n",checked.code,checked.message); std::fflush(stdout); continue; }
                std::ifstream file(path,std::ios::binary);
                if(!file) { return 66; }
                std::vector<char> image{std::istreambuf_iterator<char>{file},{}};
                auto bytes{reinterpret_cast<std::byte const*>(image.data())};
                auto verified{wsea::verify_with_policy(policy,path,bytes,image.size(),line.substr(second+1))};
                if(!verified) { std::printf("2\tDENY %s %s\n",verified.code,verified.message); std::fflush(stdout); continue; }
                if(!verified.authorization || !verified.authorization->matches(bytes,image.size())) { return 70; }
                std::puts("0\tALLOW"); std::fflush(stdout);
            }
            catch(...) { return 70; }
        }
        return std::cin.eof() ? 0 : 74;
    }
    if(argc == 2 && std::string(argv[1]) == "--identity")
    {
        std::printf("{\"architecture\":\"%s\",\"pointer_bits\":%zu,\"byte_order\":\"%s\",\"hard_float\":%s,\"openssl_number\":%lu}\n",
            target_arch(), sizeof(void*) * 8u, std::endian::native == std::endian::big ? "big" : "little",
#if defined(__ARM_PCS_VFP)
            "true",
#else
            "false",
#endif
#if defined(UWVM_WSEA_SUPPORTED)
            OpenSSL_version_num()
#else
            0ul
#endif
        );
        return 0;
    }
    if(argc != 3 && argc != 4) { return 64; }
    try
    {
        auto selected{wsea::select_module(argv[2], argv[1], argc == 4 ? argv[3] : "")};
        if(!selected) { std::printf("DENY %s %s\n", selected.code, selected.message); return 2; }
        std::ifstream source(argv[2], std::ios::binary);
        if(!source) { std::puts("HARNESS_ERROR module open failed"); return 66; }
        std::vector<char> bytes{std::istreambuf_iterator<char>{source}, {}};
        auto verified{wsea::verify_module(argv[2], reinterpret_cast<std::byte const*>(bytes.data()), bytes.size())};
        if(!verified) { std::printf("DENY %s %s\n", verified.code, verified.message); return 2; }
        if(!verified.authorization || !verified.authorization->matches(reinterpret_cast<std::byte const*>(bytes.data()), bytes.size()))
        { std::puts("HARNESS_ERROR production authorization did not match its exact source"); return 70; }
        std::puts("ALLOW"); return 0;
    }
    catch(...)
    { std::puts("HARNESS_ERROR uncaught verifier/fixture exception"); return 70; }
}
