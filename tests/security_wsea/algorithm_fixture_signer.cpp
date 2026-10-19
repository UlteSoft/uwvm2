// Test-only OpenSSL producer for forbidden Ed25519ph/context signatures.
// The candidate verifier must accept only pure Ed25519, never these variants.
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/core_names.h>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
int main(int argc, char** argv)
{
    if(argc != 6) { return 64; } // key, input, output, instance, context
    auto* file{std::fopen(argv[1], "rb")};
    if(!file) { return 66; }
    auto* key{PEM_read_PrivateKey(file, nullptr, nullptr, nullptr)}; std::fclose(file);
    auto* context{EVP_MD_CTX_new()};
    if(!key || !context) { EVP_PKEY_free(key); EVP_MD_CTX_free(context); return 70; }
    std::ifstream input(argv[2], std::ios::binary);
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>{input}, {}};
    std::string instance{argv[4]}, domain{argv[5]};
    OSSL_PARAM params[]{
        OSSL_PARAM_construct_utf8_string(OSSL_SIGNATURE_PARAM_INSTANCE, instance.data(), 0),
        OSSL_PARAM_construct_octet_string(OSSL_SIGNATURE_PARAM_CONTEXT_STRING, domain.data(), domain.size()),
        OSSL_PARAM_construct_end()};
    std::size_t length{};
    bool ok{EVP_DigestSignInit_ex(context, nullptr, nullptr, nullptr, nullptr, key, params) == 1 &&
        EVP_DigestSign(context, nullptr, &length, bytes.data(), bytes.size()) == 1};
    std::vector<unsigned char> signature(length);
    ok = ok && EVP_DigestSign(context, signature.data(), &length, bytes.data(), bytes.size()) == 1;
    EVP_MD_CTX_free(context); EVP_PKEY_free(key);
    if(!ok) { return 77; } // explicit unsupported producer; never a candidate PASS
    std::ofstream output(argv[3], std::ios::binary);
    output.write(reinterpret_cast<char const*>(signature.data()), static_cast<std::streamsize>(length));
    return output ? 0 : 74;
}
