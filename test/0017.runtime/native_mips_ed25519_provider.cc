// Genuine target OpenSSL dependency probe; no Wasm or native debug capability.
#include <fast_io.h>
#include <openssl/evp.h>
#include <array>
#include <memory>
#if !defined(__linux__) || !defined(__mips__) || __SIZEOF_POINTER__ != 8
# error This dependency probe requires MIPS N64 Linux.
#endif
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("Mips Ed25519 dependency failure line=",__LINE__); return 1; } } while(false)
int main()
{
    ::std::array<unsigned char,32u> seed{};
    for(unsigned n{}; n!=seed.size(); ++n) { seed[n]=static_cast<unsigned char>(7u+n*3u); }
    // Fixed test bytes, with an embedded NUL and high-bit bytes. No private
    // key, pointer, filesystem path or VM state appears in the output.
    ::std::array<unsigned char,7u> message{0u,1u,127u,128u,254u,255u,41u};
    using key_owner=::std::unique_ptr<::EVP_PKEY,decltype(&::EVP_PKEY_free)>;
    using context_owner=::std::unique_ptr<::EVP_MD_CTX,decltype(&::EVP_MD_CTX_free)>;
    key_owner private_key{::EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519,nullptr,seed.data(),seed.size()),::EVP_PKEY_free};
    CHECK(private_key);
    ::std::array<unsigned char,32u> public_bytes{}; auto public_size{public_bytes.size()};
    CHECK(::EVP_PKEY_get_raw_public_key(private_key.get(),public_bytes.data(),&public_size)==1 && public_size==public_bytes.size());
    key_owner public_key{::EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519,nullptr,public_bytes.data(),public_size),::EVP_PKEY_free};
    context_owner sign{::EVP_MD_CTX_new(),::EVP_MD_CTX_free},verify{::EVP_MD_CTX_new(),::EVP_MD_CTX_free};
    CHECK(public_key && sign && verify);
    CHECK(::EVP_DigestSignInit(sign.get(),nullptr,nullptr,nullptr,private_key.get())==1);
    ::std::array<unsigned char,64u> signature{}; auto signature_size{signature.size()};
    CHECK(::EVP_DigestSign(sign.get(),signature.data(),&signature_size,message.data(),message.size())==1 && signature_size==signature.size());
    CHECK(::EVP_DigestVerifyInit(verify.get(),nullptr,nullptr,nullptr,public_key.get())==1);
    CHECK(::EVP_DigestVerify(verify.get(),signature.data(),signature.size(),message.data(),message.size())==1);
    auto altered{signature}; altered[31u]^=1u;
    CHECK(::EVP_DigestVerify(verify.get(),altered.data(),altered.size(),message.data(),message.size())==0);
    message[0u]^=1u;
    CHECK(::EVP_DigestVerify(verify.get(),signature.data(),signature.size(),message.data(),message.size())==0);
    ::fast_io::io::println("PASS actual N64 OpenSSL Ed25519 sign/verify and altered signature/message rejection; Wasm-VM=false native-debug-authority=false");
}
