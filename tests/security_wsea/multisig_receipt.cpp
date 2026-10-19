// Retain a genuine multi-signature receipt while authenticated revocation of
// its ADDITIONAL signer is learned. No test-only authorization is constructed.
#include <uwvm2/utils/security/wsea.h>
#include <fstream>
#include <iterator>
#include <string>
#include <cstdio>

int main(int argc, char** argv)
{
    if(argc != 4) { return 64; }
    namespace wsea = uwvm2::utils::security::wsea;
    try
    {
        auto read = [](char const* path)
        {
            std::ifstream file(path, std::ios::binary);
            if(!file) { throw 1; }
            return std::string(std::istreambuf_iterator<char>{file}, {});
        };
        auto image = read(argv[2]);
        auto revoked = read(argv[3]);
        auto const* bytes = reinterpret_cast<std::byte const*>(image.data());
        auto const* bad = reinterpret_cast<std::byte const*>(revoked.data());
        auto verified = wsea::verify_module(argv[1], argv[2], bytes, image.size());
        if(!verified || !verified.authorization) { return 1; }
        if(verified.authorization->algorithm().find('+') == std::string_view::npos) { return 2; }
        if(!verified.authorization->matches(bytes, image.size())) { return 3; }
        auto clone = image;
        if(verified.authorization->matches(reinterpret_cast<std::byte const*>(clone.data()), clone.size())) { return 4; }
        if(verified.authorization->matches(bad, revoked.size())) { return 5; }
        auto denied = wsea::verify_module(argv[1], argv[3], bad, revoked.size());
        if(denied || denied.authorization || !denied.code || std::string_view{denied.code} != "CERT_004") { return 6; }
        if(verified.authorization->matches(bytes, image.size())) { return 7; }
        auto replay = wsea::verify_module(argv[1], argv[2], bytes, image.size());
        if(replay || replay.authorization || !replay.code || std::string_view{replay.code} != "CERT_004") { return 8; }
        std::puts("{\"status\":\"PASS\",\"checks\":8}");
        return 0;
    }
    catch(...) { return 70; }
}
