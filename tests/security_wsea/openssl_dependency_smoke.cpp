// Dependency build/link smoke only; this does not qualify VM security behavior.
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <cstdio>
#include <cstring>
int main()
{
    if(OPENSSL_VERSION_MAJOR < 3 || OpenSSL_version_num() != OPENSSL_VERSION_NUMBER) { return 2; }
    static constexpr unsigned char expected[32] = {
        0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    EVP_MD* algorithm = EVP_MD_fetch(nullptr, "SHA256", nullptr);
    unsigned char digest[EVP_MAX_MD_SIZE]{}; unsigned size{};
    bool good = algorithm && EVP_Digest("abc", 3, digest, &size, algorithm, nullptr) == 1 &&
                size == 32 && std::memcmp(digest, expected, 32) == 0;
    EVP_MD_free(algorithm);
    if(!good) { return 3; }
    std::printf("OpenSSL dependency ALLOW %s header/runtime=%lu\n", OpenSSL_version(OPENSSL_VERSION), OpenSSL_version_num());
    return 0;
}
