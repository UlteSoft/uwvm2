// Known-answer checks for the target OpenSSL digest and context-copy ABI.
#include <openssl/evp.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>

int main()
{
    struct vector { EVP_MD const* (*method)(); char const* expected; };
    vector const cases[]{
        {EVP_sha256,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {EVP_sha384,"cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7"},
        {EVP_sha512,"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"},
    };
    bool good{true};
    for(auto const& item : cases)
    {
        std::array<unsigned char,EVP_MAX_MD_SIZE> direct{},first{},second{};
        unsigned int a{},b{},c{};
        auto original{EVP_MD_CTX_new()},copy{EVP_MD_CTX_new()};
        bool ok{original && copy && EVP_Digest("abc",3,direct.data(),&a,item.method(),nullptr)==1 &&
            EVP_DigestInit_ex(original,item.method(),nullptr)==1 && EVP_DigestUpdate(original,"a",1)==1 &&
            EVP_MD_CTX_copy_ex(copy,original)==1 && EVP_DigestUpdate(original,"bc",2)==1 &&
            EVP_DigestFinal_ex(original,first.data(),&b)==1 && EVP_DigestUpdate(copy,"bc",2)==1 &&
            EVP_DigestFinal_ex(copy,second.data(),&c)==1};
        EVP_MD_CTX_free(original);EVP_MD_CTX_free(copy);
        std::array<char,2*EVP_MAX_MD_SIZE+1> hex{};
        constexpr char digits[]{"0123456789abcdef"};
        for(unsigned int i{};i<a && i<direct.size();++i)
        {hex[2*i]=digits[direct[i]>>4];hex[2*i+1]=digits[direct[i]&15];}
        ok=ok && a<=direct.size() && b<=first.size() && c<=second.size() && a==b && b==c && std::string_view{hex.data()}==item.expected &&
           std::memcmp(direct.data(),first.data(),a)==0 && std::memcmp(first.data(),second.data(),a)==0;
        std::printf("%s %s\n",ok?"PASS":"FAIL",hex.data());good=good && ok;
    }
    return good?0:1;
}
