// Embedded WSEA v2: offline-only COSE/PKI execution verifier.
// Trust policy and rollback storage belong to the trusted operating-system user,
// never to a downloader. See documents/security/wsea.md for the wire contract.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(UWVM_WSEA_OPENSSL) && (defined(__unix__) || defined(__APPLE__))
#define UWVM_WSEA_SUPPORTED 1
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509v3.h>
#endif

namespace uwvm2::utils::security::wsea
{
    class verified_module;
    struct result
    {
        char const* code{};
        char const* message{};
        ::std::shared_ptr<verified_module const> authorization{};
        explicit operator bool() const noexcept { return code == nullptr; }
    };
    inline result deny(char const* code, char const* message) noexcept { return {code, message, {}}; }
    inline constexpr ::std::uint64_t known_capabilities{63};

#if defined(UWVM_WSEA_SUPPORTED)
    namespace detail
    {
        using digest = ::std::array<unsigned char, 32>;
        template<typename T, auto Free> struct deleter { void operator()(T* p) const noexcept { if(p) { Free(p); } } };
        template<typename T, auto Free> using owned = ::std::unique_ptr<T, deleter<T, Free>>;
        struct fd
        {
            int value{-1};
            fd() noexcept = default;
            explicit fd(int v) noexcept : value{v} {}
            fd(fd const&) = delete;
            fd& operator=(fd const&) = delete;
            fd(fd&& other) noexcept : value{::std::exchange(other.value, -1)} {}
            fd& operator=(fd&& other) noexcept
            { if(this != &other) { if(value >= 0) { ::close(value); } value = ::std::exchange(other.value, -1); } return *this; }
            ~fd() { if(value >= 0) { ::close(value); } }
            explicit operator bool() const noexcept { return value >= 0; }
        };
        inline bool hash(void const* data, ::std::size_t size, digest& out) noexcept
        {
            unsigned int n{};
            return EVP_Digest(data, size, out.data(), &n, EVP_sha256(), nullptr) == 1 && n == out.size();
        }
        inline bool hash_images(::std::string_view image, ::std::size_t prefix_size, digest& prefix, digest& complete) noexcept
        {
            owned<EVP_MD_CTX,EVP_MD_CTX_free> first{EVP_MD_CTX_new()}, second{EVP_MD_CTX_new()};
            unsigned int n{}, total{};
            return prefix_size<=image.size() && first && second && EVP_DigestInit_ex(first.get(),EVP_sha256(),nullptr)==1 &&
                EVP_DigestUpdate(first.get(),image.data(),prefix_size)==1 && EVP_MD_CTX_copy_ex(second.get(),first.get())==1 &&
                EVP_DigestFinal_ex(first.get(),prefix.data(),&n)==1 && n==prefix.size() &&
                EVP_DigestUpdate(second.get(),image.data()+prefix_size,image.size()-prefix_size)==1 &&
                EVP_DigestFinal_ex(second.get(),complete.data(),&total)==1 && total==complete.size();
        }
        inline ::std::string hex(digest const& value)
        {
            constexpr char alphabet[]{"0123456789abcdef"};
            ::std::string out(64, '0');
            for(::std::size_t i{}; i != value.size(); ++i)
            { out[2*i] = alphabet[value[i] >> 4]; out[2*i+1] = alphabet[value[i] & 15]; }
            return out;
        }
        inline bool identifier(::std::string_view s) noexcept
        {
            if(s.empty() || s.size() > 255) { return false; }
            for(unsigned char c : s)
            { if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) { return false; } }
            return true;
        }
        inline bool hex_digest(::std::string_view s) noexcept
        {
            if(s.size() != 64) { return false; }
            for(char c : s) { if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { return false; } }
            return true;
        }
        inline bool number(::std::string_view s, ::std::uint64_t& n) noexcept
        {
            if(s.empty() || (s.size() > 1 && s.front() == '0')) { return false; }
            n = 0;
            for(char c : s)
            {
                if(c < '0' || c > '9' || n > (::std::numeric_limits<::std::uint64_t>::max() - static_cast<unsigned>(c-'0'))/10) { return false; }
                n = n*10 + static_cast<unsigned>(c-'0');
            }
            return true;
        }
        inline ::std::vector<::std::string_view> split(::std::string_view s, char delimiter)
        {
            ::std::vector<::std::string_view> out;
            for(;;)
            {
                auto p{s.find(delimiter)};
                out.push_back(s.substr(0, p));
                if(p == s.npos) { return out; }
                s.remove_prefix(p+1);
            }
        }
        inline bool path_text(::std::string_view p) noexcept
        {
            if(p.empty() || p.size() > 4096) { return false; }
            for(unsigned char c : p) { if(c < 32 || c == 127) { return false; } }
            return true;
        }
        inline ::std::string normalize(::std::string_view p)
        {
            if(!path_text(p)) { return {}; }
            ::std::string input;
            if(p.front() != '/')
            {
                char cwd[4097]{};
                if(!::getcwd(cwd, sizeof cwd)) { return {}; }
                input = cwd; input += '/';
            }
            input.append(p);
            ::std::vector<::std::string_view> parts;
            for(auto segment : split(input, '/'))
            {
                if(segment.empty() || segment == ".") { continue; }
                if(segment == "..") { if(!parts.empty()) { parts.pop_back(); } }
                else { parts.push_back(segment); }
            }
            ::std::string out;
            for(auto part : parts) { out += '/'; out.append(part); }
            return out.empty() ? "/" : out;
        }
        inline bool trusted_stat(struct stat const& st, bool directory) noexcept
        {
            if(directory ? !S_ISDIR(st.st_mode) : !S_ISREG(st.st_mode)) { return false; }
            if(st.st_uid != 0 && st.st_uid != ::geteuid()) { return false; }
            // A root-owned sticky /tmp ancestor cannot rename another user's directory.
            if((st.st_mode & 0022) != 0 && !(directory && st.st_uid == 0 && (st.st_mode & S_ISVTX))) { return false; }
            return directory || st.st_nlink == 1;
        }
        inline fd protected_open(::std::string_view path, bool directory)
        {
            if(!path_text(path) || path.front() != '/') { return {}; }
            fd current{::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC)};
            if(!current) { return {}; }
            auto parts{split(path.substr(1), '/')};
            for(::std::size_t i{}; i != parts.size(); ++i)
            {
                auto part{parts[i]};
                if(part.empty() || part == "." || part == "..") { return {}; }
                bool dir{i+1 != parts.size() || directory};
                ::std::string name{part};
                fd next{::openat(current.value, name.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | (dir ? O_DIRECTORY : O_NONBLOCK))};
                struct stat st{};
                if(!next || ::fstat(next.value, &st) != 0 || !trusted_stat(st, dir)) { return {}; }
                current = ::std::move(next);
            }
            return current;
        }
        inline bool read_fd(int file, ::std::size_t limit, ::std::string& out)
        {
            struct stat before{};
            if(::fstat(file, &before) != 0 || !S_ISREG(before.st_mode) || before.st_size < 0 || static_cast<::std::uintmax_t>(before.st_size) > limit) { return false; }
            out.resize(static_cast<::std::size_t>(before.st_size));
            ::std::size_t done{};
            while(done != out.size())
            {
                auto n{::read(file, out.data()+done, out.size()-done)};
                if(n < 0 && errno == EINTR) { continue; }
                if(n <= 0) { return false; }
                done += static_cast<::std::size_t>(n);
            }
            char extra{};
            if(::read(file, &extra, 1) != 0) { return false; }
            struct stat after{};
            return ::fstat(file, &after) == 0 && before.st_dev == after.st_dev && before.st_ino == after.st_ino &&
                before.st_size == after.st_size && before.st_mtime == after.st_mtime && before.st_ctime == after.st_ctime;
        }
        inline bool material(::std::string const& path, ::std::size_t limit, ::std::string& out)
        {
            fd file{::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK)};
            return file && read_fd(file.value, limit, out);
        }
        enum class signature_kind { ecdsa, ed25519, rsa_pss, rsa_pkcs1 };
        struct signature_profile
        {
            ::std::string_view name;
            int cose_id;
            signature_kind kind;
            int curve_nid, hash_nid;
            ::std::size_t component_bytes;
        };
        // IDs and exact curve bindings follow RFC 9053/9864. Legacy ES256 is
        // retained for old six-field policies; no policy is silently widened.
        inline constexpr ::std::array<signature_profile,13> signature_profiles{{
            {"ES256",-7,signature_kind::ecdsa,NID_X9_62_prime256v1,NID_sha256,32},
            {"ESP256",-9,signature_kind::ecdsa,NID_X9_62_prime256v1,NID_sha256,32},
            {"ES384",-35,signature_kind::ecdsa,NID_secp384r1,NID_sha384,48},
            {"ESP384",-51,signature_kind::ecdsa,NID_secp384r1,NID_sha384,48},
            {"ES512",-36,signature_kind::ecdsa,NID_secp521r1,NID_sha512,66},
            {"ESP512",-52,signature_kind::ecdsa,NID_secp521r1,NID_sha512,66},
            {"Ed25519",-19,signature_kind::ed25519,0,0,32},
            {"PS256",-37,signature_kind::rsa_pss,0,NID_sha256,0},
            {"PS384",-38,signature_kind::rsa_pss,0,NID_sha384,0},
            {"PS512",-39,signature_kind::rsa_pss,0,NID_sha512,0},
            {"RS256",-257,signature_kind::rsa_pkcs1,0,NID_sha256,0},
            {"RS384",-258,signature_kind::rsa_pkcs1,0,NID_sha384,0},
            {"RS512",-259,signature_kind::rsa_pkcs1,0,NID_sha512,0}
        }};
        inline signature_profile const* profile_by_id(int id) noexcept
        { for(auto const& p : signature_profiles) { if(p.cose_id==id) { return &p; } } return nullptr; }
        inline ::std::uint64_t profile_bit(signature_profile const& profile) noexcept
        {
            for(::std::size_t i{}; i!=signature_profiles.size(); ++i)
            { if(signature_profiles[i].cose_id==profile.cose_id) { return ::std::uint64_t{1}<<i; } }
            return 0;
        }
        inline bool algorithm_list(::std::string_view text, ::std::uint64_t& mask)
        {
            mask=0;
            auto names{split(text,',')};
            if(names.empty() || names.size()>signature_profiles.size()) { return false; }
            for(auto name : names)
            {
                ::std::uint64_t bit{};
                for(auto const& p : signature_profiles) { if(name==p.name) { bit=profile_bit(p); break; } }
                if(!bit || (mask&bit)) { return false; }
                mask|=bit;
            }
            return mask!=0;
        }
        inline constexpr ::std::size_t max_signatures{8};
        struct rule
        {
            ::std::string id, abi, signer;
            ::std::uint64_t version{}, epoch{}, capabilities{};
            ::std::uint64_t algorithms{1}; // ES256 only unless explicitly configured
            ::std::vector<::std::string> additional_signers;
            ::std::uint64_t threshold{1};
            bool threshold_set{};
            bool accepts(::std::string_view pin) const noexcept
            {
                if(signer==pin) { return true; }
                for(auto const& item : additional_signers) { if(item==pin) { return true; } }
                return false;
            }
        };
        struct policy
        {
            ::std::uint64_t version{}, max_age{}, max_size{};
            ::std::string config_path, root_path, state_path, root_pem, fingerprint;
            ::std::vector<rule> rules;
        };
        inline result load_policy(::std::string_view path, ::std::shared_ptr<policy const>& output)
        {
            auto p{::std::make_shared<policy>()};
            p->config_path=path;
            auto file{protected_open(path, false)};
            ::std::string text;
            if(!file || !read_fd(file.value, 65536, text)) { return deny("POL_002", "Policy must be a bounded, OS-protected regular file with trusted ancestors"); }
            if(text.empty() || text.back() != '\n') { return deny("POL_002", "Malformed policy"); }
            auto lines{split(text, '\n')};
            if(lines.empty() || lines.front() != "UWVM-WSEA-POLICY-1") { return deny("POL_002", "Unsupported policy schema"); }
            unsigned fields{};
            for(::std::size_t i{1}; i+1 < lines.size(); ++i)
            {
                auto line{lines[i]}; auto equal{line.find('=')};
                if(equal == line.npos) { return deny("POL_002", "Malformed policy field"); }
                auto key{line.substr(0,equal)}, value{line.substr(equal+1)};
                unsigned field{}; bool valid{};
                if(key == "policy_version") { field=1; valid=number(value,p->version) && p->version>0; }
                else if(key == "trust_root") { field=2; valid=path_text(value) && value.front()=='/'; p->root_path=value; }
                else if(key == "state_dir") { field=4; valid=path_text(value) && value.front()=='/'; p->state_path=value; }
                else if(key == "max_crl_age_seconds") { field=8; valid=number(value,p->max_age) && p->max_age>0 && p->max_age<=31536000; }
                else if(key == "max_module_bytes") { field=16; valid=number(value,p->max_size) && p->max_size>=8 && p->max_size<=1073741824; }
                else if(key == "module")
                {
                    auto values{split(value,'|')}; rule r;
                    if((values.size()!=6 && values.size()!=7) || !identifier(values[0]) || values[1]!="uwvm2" || !hex_digest(values[2]) ||
                       !number(values[3],r.version) || r.version==0 || !number(values[4],r.epoch) || !number(values[5],r.capabilities) ||
                       (r.capabilities & ~known_capabilities) || p->rules.size()>=1024) { return deny("POL_002", "Invalid module authorization"); }
                    if(values.size()==7 && !algorithm_list(values[6],r.algorithms)) { return deny("POL_002","Invalid or duplicate signature algorithm allowlist"); }
                    r.id=values[0]; r.abi=values[1]; r.signer=values[2];
                    for(auto const& old : p->rules) { if(old.id==r.id) { return deny("POL_002", "Duplicate module authorization"); } }
                    p->rules.push_back(::std::move(r)); continue;
                }
                else if(key=="module_signer" || key=="module_threshold")
                {
                    auto values{split(value,'|')};
                    if(values.size()!=2) { return deny("POL_002","Malformed multi-signature policy"); }
                    rule* selected{};
                    for(auto& r : p->rules) { if(r.id==values[0]) { selected=&r; break; } }
                    if(!selected) { return deny("POL_002","Unknown multi-signature module identity"); }
                    if(key=="module_signer")
                    {
                        if(!hex_digest(values[1]) || selected->accepts(values[1]) || selected->additional_signers.size()>=max_signatures-1)
                        { return deny("POL_002","Invalid, duplicate or excessive authorized signer"); }
                        selected->additional_signers.emplace_back(values[1]);
                    }
                    else
                    {
                        if(selected->threshold_set || !number(values[1],selected->threshold) || selected->threshold==0 || selected->threshold>max_signatures)
                        { return deny("POL_002","Invalid or duplicate signature threshold"); }
                        selected->threshold_set=true;
                    }
                    continue;
                }
                else { return deny("POL_002", "Unknown policy field"); }
                if(!valid || (fields & field)) { return deny("POL_002", "Invalid or duplicate policy field"); }
                fields |= field;
            }
            if(fields!=31 || p->rules.empty()) { return deny("POL_002", "Incomplete policy"); }
            for(auto const& r : p->rules)
            { if(r.threshold>1+r.additional_signers.size()) { return deny("POL_002","Signature threshold exceeds distinct authorized keys"); } }
            auto root{protected_open(p->root_path,false)};
            if(!root || !read_fd(root.value,4194304,p->root_pem)) { return deny("CERT_001", "Trust root is not OS-protected"); }
            auto joined{text}; joined += p->root_pem; digest d{};
            if(!hash(joined.data(),joined.size(),d)) { return deny("SIG_005", "SHA-256 unavailable"); }
            p->fingerprint=hex(d); output=::std::move(p); return {};
        }
        struct selection { ::std::string module, policy_path, expected_id; ::std::shared_ptr<policy const> config; };
        inline ::std::mutex registry_mutex;
        inline ::std::vector<selection> selections;
        inline ::std::shared_ptr<policy const> default_policy;

        // Only a fixed deterministic CBOR subset is accepted: no maps other than
        // the exact protected header, no indefinite lengths or non-minimal integers.
        struct cbor
        {
            ::std::string_view bytes;
            bool head(unsigned major, ::std::uint64_t& n) noexcept
            {
                if(bytes.empty()) { return false; }
                auto initial{static_cast<unsigned char>(bytes.front())}; bytes.remove_prefix(1);
                if((initial>>5)!=major) { return false; }
                unsigned info{initial&31u};
                if(info<24) { n=info; return true; }
                unsigned count{info==24 ? 1u : info==25 ? 2u : info==26 ? 4u : info==27 ? 8u : 0u};
                if(count==0 || bytes.size()<count) { return false; }
                n=0; for(unsigned i{}; i!=count; ++i) { n=(n<<8)|static_cast<unsigned char>(bytes[i]); }
                bytes.remove_prefix(count);
                return n >= (count==1 ? 24ull : count==2 ? 256ull : count==4 ? 65536ull : 4294967296ull);
            }
            bool string(unsigned major, ::std::string_view& value) noexcept
            {
                ::std::uint64_t size{};
                if(!head(major,size) || size>bytes.size()) { return false; }
                value=bytes.substr(0,static_cast<::std::size_t>(size)); bytes.remove_prefix(static_cast<::std::size_t>(size)); return true;
            }
            bool exact(unsigned char c) noexcept
            { if(bytes.empty() || static_cast<unsigned char>(bytes.front())!=c) { return false; } bytes.remove_prefix(1); return true; }
        };
        inline void append_head(::std::string& out, unsigned major, ::std::uint64_t n)
        {
            if(n<24) { out+=static_cast<char>((major<<5)|n); return; }
            unsigned count{n<=255 ? 1u : n<=65535 ? 2u : n<=4294967295ull ? 4u : 8u};
            out+=static_cast<char>((major<<5)|(count==1 ? 24 : count==2 ? 25 : count==4 ? 26 : 27));
            for(unsigned i{count}; i; --i) { out+=static_cast<char>(n>>((i-1)*8)); }
        }
        struct manifest
        {
            ::std::string_view payload, signature, id, abi, hash, protected_headers;
            signature_profile const* algorithm{};
            ::std::uint64_t version{}, epoch{}, size{}, before{}, after{}, capabilities{};
            ::std::vector<::std::string> signers; // authenticated distinct SPKIs, never decoded from the manifest
        };
        inline bool decode(::std::string_view input, manifest& m) noexcept
        {
            cbor outer{input};
            if(!outer.exact(0xd2) || !outer.exact(0x84) || !outer.string(2,m.protected_headers)) { return false; }
            cbor header{m.protected_headers}; ::std::uint64_t id{};
            if(!header.exact(0xa1) || !header.exact(0x01) || !header.head(1,id) || id>65534 || !header.bytes.empty() ||
               !(m.algorithm=profile_by_id(-static_cast<int>(id)-1)) || !outer.exact(0xa0) ||
               !outer.string(2,m.payload) || !outer.string(2,m.signature) || !outer.bytes.empty()) { return false; }
            auto const kind{m.algorithm->kind};
            if(kind==signature_kind::ecdsa && m.signature.size()!=2*m.algorithm->component_bytes) { return false; }
            if(kind==signature_kind::ed25519 && m.signature.size()!=64) { return false; }
            if((kind==signature_kind::rsa_pss || kind==signature_kind::rsa_pkcs1) && (m.signature.size()<384 || m.signature.size()>1024)) { return false; }
            cbor inner{m.payload}; ::std::string_view schema;
            return inner.exact(0x8a) && inner.string(3,schema) && schema=="uwvm-wsea-v2" && inner.string(3,m.id) && identifier(m.id) &&
                inner.head(0,m.version) && m.version>0 && inner.head(0,m.epoch) && inner.head(0,m.size) && m.size>=8 &&
                inner.string(2,m.hash) && m.hash.size()==32 && inner.head(0,m.before) && inner.head(0,m.after) &&
                m.before<m.after && m.after<=static_cast<::std::uint64_t>(INT64_MAX) && inner.string(3,m.abi) && m.abi=="uwvm2" &&
                inner.head(0,m.capabilities) && !(m.capabilities & ~known_capabilities) && inner.bytes.empty();
        }
        inline constexpr ::std::string_view embedded_section_name{"uwvm.wsea.v2"};
        inline constexpr ::std::size_t max_embedded_evidence{16*1024*1024};
        inline bool wasm_u32(::std::string_view input, ::std::size_t& offset, ::std::uint32_t& out, bool canonical=false) noexcept
        {
            out=0; unsigned count{};
            for(; count!=5; ++count)
            {
                if(offset==input.size()) { return false; }
                auto byte{static_cast<unsigned char>(input[offset++])};
                if(count==4 && byte>15) { return false; }
                out|=static_cast<::std::uint32_t>(byte&127)<<(7*count);
                if(!(byte&128)) { return !canonical || count==0 || (byte&127)!=0; }
            }
            return false;
        }
        struct signature_evidence { ::std::string_view cose, certificates, crls; };
        struct embedded_evidence
        {
            ::std::string_view prefix, payload, cose, certificates, crls;
            ::std::vector<signature_evidence> signatures;
        };
        inline result extract_embedded(::std::string_view image, embedded_evidence& out) noexcept
        {
            if(image.size()<8) { return deny("VM_001","Truncated WebAssembly header"); }
            for(::std::size_t offset{8}; offset<image.size();)
            {
                auto start{offset}; auto id{static_cast<unsigned char>(image[offset++])};
                auto length_start{offset}; ::std::uint32_t size{};
                if(!wasm_u32(image,offset,size) || size>image.size()-offset) { return deny("SIG_002","Malformed WebAssembly section boundary"); }
                auto content{offset}, end{offset+size};
                if(id==0)
                {
                    ::std::uint32_t name_size{}; auto name_start{offset};
                    if(!wasm_u32(image.substr(0,end),offset,name_size) || name_size>end-offset) { return deny("SIG_002","Malformed custom-section name"); }
                    auto name{image.substr(offset,name_size)}; offset+=name_size;
                    if(name==embedded_section_name)
                    {
                        ::std::uint32_t repeated{}; auto check_length{length_start}, check_name{name_start};
                        if(end!=image.size() || !wasm_u32(image,check_length,repeated,true) || check_length!=content ||
                           !wasm_u32(image,check_name,repeated,true) || check_name+name_size!=offset || end-offset>max_embedded_evidence)
                        { return deny("SIG_002","WSEA must be one canonical final custom section"); }
                        cbor bundle{image.substr(offset,end-offset)};
                        ::std::uint64_t count{},version{},total{1};
                        if(!bundle.head(4,count) || !bundle.head(0,version) || !((version==1 && count==4) || (version==2 && count==2)) ||
                           (version==2 && (!bundle.head(4,total) || total<2 || total>max_signatures)))
                        { return deny("SIG_002","Malformed bounded WSEA signature list"); }
                        for(::std::uint64_t i{}; i<total; ++i)
                        {
                            signature_evidence entry;
                            if((version==2 && (!bundle.head(4,count) || count!=3)) ||
                               !bundle.string(2,entry.cose) || entry.cose.empty() || entry.cose.size()>65536 ||
                               !bundle.string(2,entry.certificates) || entry.certificates.empty() || entry.certificates.size()>4194304 ||
                               !bundle.string(2,entry.crls) || entry.crls.empty() || entry.crls.size()>4194304)
                            { return deny("SIG_002","Malformed bounded signature contribution"); }
                            for(auto const& prior : out.signatures)
                            { if(prior.cose==entry.cose) { return deny("SIG_002","Duplicate signature contribution"); } }
                            out.signatures.push_back(entry);
                        }
                        if(!bundle.bytes.empty()) { return deny("SIG_002","Trailing embedded evidence"); }
                        out.cose=out.signatures.front().cose;
                        out.certificates=out.signatures.front().certificates;
                        out.crls=out.signatures.front().crls;
                        out.payload=image.substr(offset,end-offset);
                        out.prefix=image.substr(0,start); return {};
                    }
                }
                offset=end;
            }
            return deny("SIG_001","Missing embedded WSEA custom section");
        }
        inline bool pem_blocks(::std::string_view input, ::std::string_view label, ::std::vector<::std::string_view>& blocks)
        {
            ::std::string begin{"-----BEGIN "}; begin+=label; begin+="-----";
            ::std::string end{"-----END "}; end+=label; end+="-----";
            while(!input.empty())
            {
                while(!input.empty() && (input.front()=='\n' || input.front()=='\r' || input.front()==' ' || input.front()=='\t')) { input.remove_prefix(1); }
                if(input.empty()) { break; }
                if(!input.starts_with(begin) || blocks.size()>=16) { return false; }
                auto e{input.find(end,begin.size())};
                if(e==input.npos) { return false; }
                auto nested{input.find("-----BEGIN ",begin.size())};
                if(nested!=input.npos && nested<e) { return false; }
                e+=end.size(); blocks.push_back(input.substr(0,e)); input.remove_prefix(e);
            }
            return !blocks.empty();
        }
        inline void openssl_release(void* allocation) noexcept { OPENSSL_free(allocation); }
        inline void free_asn1_fields(ASN1_SEQUENCE_ANY* fields) noexcept { sk_ASN1_TYPE_pop_free(fields,ASN1_TYPE_free); }
        inline owned<ASN1_SEQUENCE_ANY,free_asn1_fields> asn1_fields(unsigned char const* data, long size)
        {
            auto cursor{data};
            owned<ASN1_SEQUENCE_ANY,free_asn1_fields> result{d2i_ASN1_SEQUENCE_ANY(nullptr,&cursor,size)};
            if(!result || cursor!=data+size) { return {}; } return result;
        }
        inline owned<ASN1_SEQUENCE_ANY,free_asn1_fields> nested_fields(ASN1_TYPE const* value)
        {
            if(!value || value->type!=V_ASN1_SEQUENCE) { return {}; }
            int size{i2d_ASN1_TYPE(value,nullptr)};
            if(size<=0 || size>4194304) { return {}; }
            ::std::vector<unsigned char> data(static_cast<::std::size_t>(size)); auto output{data.data()};
            if(i2d_ASN1_TYPE(value,&output)!=size) { return {}; }
            return asn1_fields(data.data(),size);
        }
        inline bool certificate_key_shape(unsigned char const* data, long size)
        {
            // X509 decoding opportunistically imports SPKI keys. Admit only
            // bounded named-key encodings BEFORE it can parse arbitrary EC
            // parameters or unbounded RSA integers. This is ASN.1 grammar,
            // not replacement cryptography.
            auto certificate{asn1_fields(data,size)};
            if(!certificate || sk_ASN1_TYPE_num(certificate.get())!=3) { return false; }
            auto tbs{nested_fields(sk_ASN1_TYPE_value(certificate.get(),0))};
            if(!tbs || sk_ASN1_TYPE_num(tbs.get())<6) { return false; }
            auto first{sk_ASN1_TYPE_value(tbs.get(),0)};
            int key_index{5};
            if(first->type!=V_ASN1_INTEGER)
            {
                int length{i2d_ASN1_TYPE(first,nullptr)};
                ::std::array<unsigned char,16> version{}; auto output{version.data()};
                if(length<=0 || length>static_cast<int>(version.size()) || i2d_ASN1_TYPE(first,&output)!=length || version[0]!=0xa0) { return false; }
                key_index=6;
            }
            if(sk_ASN1_TYPE_num(tbs.get())<=key_index) { return false; }
            auto spki{nested_fields(sk_ASN1_TYPE_value(tbs.get(),key_index))};
            if(!spki || sk_ASN1_TYPE_num(spki.get())!=2) { return false; }
            auto ai{sk_ASN1_TYPE_value(spki.get(),0)}, bits{sk_ASN1_TYPE_value(spki.get(),1)};
            if(!ai || !bits || ai->type!=V_ASN1_SEQUENCE || bits->type!=V_ASN1_BIT_STRING || (bits->value.bit_string->flags&7)!=0) { return false; }
            int ai_size{i2d_ASN1_TYPE(ai,nullptr)};
            if(ai_size<=0 || ai_size>128) { return false; }
            ::std::array<unsigned char,128> encoded{}; auto output{encoded.data()};
            if(i2d_ASN1_TYPE(ai,&output)!=ai_size) { return false; }
            auto cursor{static_cast<unsigned char const*>(encoded.data())};
            owned<X509_ALGOR,X509_ALGOR_free> algorithm{d2i_X509_ALGOR(nullptr,&cursor,ai_size)};
            if(!algorithm || cursor!=encoded.data()+ai_size) { return false; }
            ASN1_OBJECT const* object{}; int parameter_type{}; void const* parameter{};
            X509_ALGOR_get0(&object,&parameter_type,&parameter,algorithm.get());
            auto public_key{bits->value.bit_string}; auto public_bytes{ASN1_STRING_get0_data(public_key)};
            int public_size{ASN1_STRING_length(public_key)}, nid{OBJ_obj2nid(object)};
            if(!public_bytes) { return false; }
            if(nid==NID_rsaEncryption) { return parameter_type==V_ASN1_NULL && public_size>=384 && public_size<=1040; }
            if(nid==NID_ED25519) { return parameter_type==V_ASN1_UNDEF && public_size==32; }
            if(nid!=NID_X9_62_id_ecPublicKey || parameter_type!=V_ASN1_OBJECT || !parameter) { return false; }
            int curve{OBJ_obj2nid(static_cast<ASN1_OBJECT const*>(parameter))};
            int width{curve==NID_X9_62_prime256v1 ? 32 : curve==NID_secp384r1 ? 48 : curve==NID_secp521r1 ? 66 : 0};
            return width && public_size==1+2*width && public_bytes[0]==4;
        }
        template<typename T, auto Decode, auto Encode, auto Free>
        inline owned<T,Free> pem_object(::std::string_view block, char const* expected_label)
        {
            owned<BIO,BIO_free> bio{BIO_new_mem_buf(block.data(),static_cast<int>(block.size()))};
            char* raw_label{}; char* raw_header{}; unsigned char* raw_der{}; long length{};
            int parsed{bio ? PEM_read_bio(bio.get(),&raw_label,&raw_header,&raw_der,&length) : 0};
            owned<char,openssl_release> label{raw_label}, header{raw_header};
            owned<unsigned char,openssl_release> der{raw_der};
            if(parsed!=1 || !label || !header || !der || length<=0 || length>4194304 ||
               ::std::strcmp(label.get(),expected_label)!=0 || *header!='\0') { return {}; }
            if(::std::strcmp(expected_label,"CERTIFICATE")==0 && !certificate_key_shape(der.get(),length)) { return {}; }
            auto cursor{static_cast<unsigned char const*>(der.get())};
            owned<T,Free> object{Decode(nullptr,&cursor,length)};
            if(!object || cursor!=der.get()+length || Encode(object.get(),nullptr)!=length) { return {}; }
            ::std::vector<unsigned char> canonical(static_cast<::std::size_t>(length)); auto output{canonical.data()};
            if(Encode(object.get(),&output)!=length || ::std::memcmp(der.get(),canonical.data(),canonical.size())!=0) { return {}; }
            return object;
        }
        inline int ec_curve(EVP_PKEY* key) noexcept
        {
            if(!key || !EVP_PKEY_is_a(key,"EC")) { return NID_undef; }
            char group[80]{}; ::std::size_t length{};
            if(EVP_PKEY_get_utf8_string_param(key,OSSL_PKEY_PARAM_GROUP_NAME,group,sizeof group,&length)!=1) { return NID_undef; }
            if(::std::strcmp(group,"prime256v1")==0 || ::std::strcmp(group,"P-256")==0) { return NID_X9_62_prime256v1; }
            if(::std::strcmp(group,"secp384r1")==0 || ::std::strcmp(group,"P-384")==0) { return NID_secp384r1; }
            if(::std::strcmp(group,"secp521r1")==0 || ::std::strcmp(group,"P-521")==0) { return NID_secp521r1; }
            return NID_undef;
        }
        inline bool little_endian_less(unsigned char const* value, ::std::array<unsigned char,32> const& limit) noexcept
        {
            for(::std::size_t i{32}; i; --i)
            { if(value[i-1]!=limit[i-1]) { return value[i-1]<limit[i-1]; } }
            return false;
        }
        inline bool admitted_ed25519_point(unsigned char const* encoded) noexcept
        {
            // Encoding checks only; the cryptographic library still checks the
            // curve and signature equation. The five canonical small-order y
            // encodings are the finite set documented by libsodium ref10.
            ::std::array<unsigned char,32> y{};
            ::std::memcpy(y.data(),encoded,y.size()); y[31]&=0x7f;
            constexpr auto prime{[] { ::std::array<unsigned char,32> p{}; p.fill(0xff); p[0]=0xed; p[31]=0x7f; return p; }()};
            if(!little_endian_less(y.data(),prime)) { return false; }
            constexpr ::std::array<::std::array<unsigned char,32>,5> small{{
                {}, {1},
                {0xec,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
                 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f},
                {0x26,0xe8,0x95,0x8f,0xc2,0xb2,0x27,0xb0,0x45,0xc3,0xf4,0x89,0xf2,0xef,0x98,0xf0,
                 0xd5,0xdf,0xac,0x05,0xd3,0xc6,0x33,0x39,0xb1,0x38,0x02,0x88,0x6d,0x53,0xfc,0x05},
                {0xc7,0x17,0x6a,0x70,0x3d,0x4d,0xd8,0x4f,0xba,0x3c,0x0b,0x76,0x0d,0x10,0x67,0x0f,
                 0x2a,0x20,0x53,0xfa,0x2c,0x39,0xcc,0xc6,0x4e,0xc7,0xfd,0x77,0x92,0xac,0x03,0x7a}
            }};
            for(auto const& point : small) { if(y==point) { return false; } }
            return true;
        }
        inline bool admitted_ed25519_signature(unsigned char const* signature, ::std::size_t size) noexcept
        {
            constexpr ::std::array<unsigned char,32> order{{
                0xed,0xd3,0xf5,0x5c,0x1a,0x63,0x12,0x58,0xd6,0x9c,0xf7,0xa2,0xde,0xf9,0xde,0x14,
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x10}};
            return signature && size==64 && admitted_ed25519_point(signature) && little_endian_less(signature+32,order);
        }
        struct rsa_public_validation_cache
        {
            ::std::mutex mutex;
            ::std::array<digest,64> identities{};
            ::std::size_t count{}, next{};
            bool contains(digest const& identity) noexcept
            {
                ::std::lock_guard lock{mutex};
                for(::std::size_t i{};i<count;++i) { if(identities[i]==identity) { return true; } }
                return false;
            }
            void remember(digest const& identity) noexcept
            {
                ::std::lock_guard lock{mutex};
                for(::std::size_t i{};i<count;++i) { if(identities[i]==identity) { return; } }
                if(count<identities.size()) { identities[count++]=identity; }
                else { identities[next]=identity; next=(next+1)%identities.size(); }
            }
        };
        inline rsa_public_validation_cache rsa_public_validations;
        inline bool strong_key(EVP_PKEY* key, [[maybe_unused]] bool leaf, digest const* identity=nullptr) noexcept
        {
            if(!key) { return false; }
            bool supported{ec_curve(key)!=NID_undef || EVP_PKEY_is_a(key,"ED25519")};
            if(EVP_PKEY_is_a(key,"ED25519"))
            {
                ::std::array<unsigned char,32> encoded{}; ::std::size_t size{encoded.size()};
                if(EVP_PKEY_get_raw_public_key(key,encoded.data(),&size)!=1 || size!=encoded.size() || !admitted_ed25519_point(encoded.data())) { return false; }
            }
            bool rsa{EVP_PKEY_is_a(key,"RSA") && !EVP_PKEY_is_a(key,"RSA-PSS")};
            if(rsa)
            {
                BIGNUM* raw_exponent{};
                if(EVP_PKEY_get_bits(key)<3072 || EVP_PKEY_get_bits(key)>8192 ||
                   EVP_PKEY_get_bn_param(key,OSSL_PKEY_PARAM_RSA_E,&raw_exponent)!=1) { return false; }
                owned<BIGNUM,BN_free> exponent{raw_exponent};
                supported=exponent && BN_is_word(exponent.get(),65537);
            }
            if(!supported) { return false; }
            // This memoizes only OpenSSL's expensive mathematical RSA public-key
            // validation, keyed by the exact certificate SPKI. These immutable
            // bytes come from this load's privately parsed certificate. It does
            // not attest a signer, certificate, signature, policy or revocation.
            // Structural/profile checks above and every authorization check in
            // verify_crypto still run. Nothing is persisted across processes.
            if(rsa && identity && rsa_public_validations.contains(*identity)) { return true; }
            owned<EVP_PKEY_CTX,EVP_PKEY_CTX_free> check{EVP_PKEY_CTX_new_from_pkey(nullptr,key,nullptr)};
            if(!check || EVP_PKEY_public_check(check.get())!=1) { return false; }
            if(rsa && identity) { rsa_public_validations.remember(*identity); }
            return true;
        }
        inline bool strong_certificate_key(X509* certificate, EVP_PKEY* key, bool leaf) noexcept
        {
            auto spki{X509_get_X509_PUBKEY(certificate)};
            auto encoded_key{X509_get0_pubkey_bitstr(certificate)};
            if(!encoded_key || (encoded_key->flags&7)!=0) { return false; }
            ASN1_OBJECT* object{}; X509_ALGOR* algorithm{};
            unsigned char const* public_bytes{}; int public_size{};
            if(!spki || X509_PUBKEY_get0_param(&object,&public_bytes,&public_size,&algorithm,spki)!=1 || !object || !algorithm || !public_bytes) { return false; }
            int parameter_type{}; void const* parameters{};
            X509_ALGOR_get0(nullptr,&parameter_type,&parameters,algorithm);
            int nid{OBJ_obj2nid(object)};
            // Keep the SPKI grammar identical to crypto/x509: rsaEncryption
            // has explicit NULL, EC uses a named curve OID, Ed25519 has none.
            // Signature AlgorithmIdentifiers have their separate grammar above.
            if(nid==NID_rsaEncryption)
            {
                if(parameter_type!=V_ASN1_NULL || key!=X509_get0_pubkey(certificate)) { return false; }
                // Keep the original SPKI encoding, rather than normalizing an
                // extracted EVP key: distinct malformed encodings must never
                // borrow an earlier key's validation result.
                ::std::array<unsigned char,2048> encoded{};
                int size{i2d_X509_PUBKEY(spki,nullptr)};
                if(size<=0 || static_cast<::std::size_t>(size)>encoded.size()) { return false; }
                auto cursor{encoded.data()}; digest identity{};
                if(i2d_X509_PUBKEY(spki,&cursor)!=size || !hash(encoded.data(),static_cast<::std::size_t>(size),identity)) { return false; }
                return strong_key(key,leaf,&identity);
            }
            else if(nid==NID_X9_62_id_ecPublicKey)
            {
                int curve{ec_curve(key)};
                int width{curve==NID_X9_62_prime256v1 ? 32 : curve==NID_secp384r1 ? 48 : curve==NID_secp521r1 ? 66 : 0};
                if(parameter_type!=V_ASN1_OBJECT || !parameters || !width || public_size!=1+2*width || public_bytes[0]!=4 ||
                   OBJ_obj2nid(static_cast<ASN1_OBJECT const*>(parameters))!=curve) { return false; }
            }
            else if(nid==NID_ED25519)
            { if(parameter_type!=V_ASN1_UNDEF || public_size!=32) { return false; } }
            else { return false; }
            return strong_key(key,leaf);
        }
        inline int sha2_algorithm(X509_ALGOR const* algorithm) noexcept
        {
            if(!algorithm) { return NID_undef; }
            ASN1_OBJECT const* object{}; int type{};
            X509_ALGOR_get0(&object,&type,nullptr,algorithm);
            int nid{OBJ_obj2nid(object)};
            return (type==V_ASN1_UNDEF || type==V_ASN1_NULL) && (nid==NID_sha256 || nid==NID_sha384 || nid==NID_sha512) ? nid : NID_undef;
        }
        inline bool strong_signature(X509_ALGOR const* algorithm, ASN1_BIT_STRING const* signature) noexcept
        {
            if(!algorithm) { return false; }
            ASN1_OBJECT const* object{}; void const* parameters{}; int type{};
            X509_ALGOR_get0(&object,&type,&parameters,algorithm);
            int nid{OBJ_obj2nid(object)};
            if(nid==NID_ED25519)
            {
                return type==V_ASN1_UNDEF && signature && (signature->flags&7)==0 &&
                    admitted_ed25519_signature(ASN1_STRING_get0_data(signature),static_cast<::std::size_t>(ASN1_STRING_length(signature)));
            }
            if(nid==NID_ecdsa_with_SHA256 || nid==NID_ecdsa_with_SHA384 || nid==NID_ecdsa_with_SHA512)
            { return type==V_ASN1_UNDEF; }
            if(nid==NID_sha256WithRSAEncryption || nid==NID_sha384WithRSAEncryption || nid==NID_sha512WithRSAEncryption)
            { return type==V_ASN1_UNDEF || type==V_ASN1_NULL; }
            if(nid!=NID_rsassaPss || type!=V_ASN1_SEQUENCE || !parameters) { return false; }
            auto sequence{static_cast<ASN1_STRING const*>(parameters)};
            int size{ASN1_STRING_length(sequence)};
            auto start{ASN1_STRING_get0_data(sequence)}, cursor{start};
            if(size<=0 || size>512 || !start) { return false; }
            owned<RSA_PSS_PARAMS,RSA_PSS_PARAMS_free> pss{d2i_RSA_PSS_PARAMS(nullptr,&cursor,size)};
            if(!pss || cursor!=start+size) { return false; }
            ::std::array<unsigned char,512> canonical{}; auto encoded{canonical.data()};
            int canonical_size{i2d_RSA_PSS_PARAMS(pss.get(),nullptr)};
            if(canonical_size!=size || canonical_size>static_cast<int>(canonical.size()) ||
               i2d_RSA_PSS_PARAMS(pss.get(),&encoded)!=size || ::std::memcmp(start,canonical.data(),static_cast<::std::size_t>(size))!=0)
            { return false; }
            int hash_nid{sha2_algorithm(pss->hashAlgorithm)};
            if(hash_nid==NID_undef || !pss->maskGenAlgorithm || !pss->saltLength) { return false; }
            ASN1_OBJECT const* mgf_object{}; void const* mgf_parameters{}; int mgf_type{};
            X509_ALGOR_get0(&mgf_object,&mgf_type,&mgf_parameters,pss->maskGenAlgorithm);
            if(OBJ_obj2nid(mgf_object)!=NID_mgf1 || mgf_type!=V_ASN1_SEQUENCE || !mgf_parameters) { return false; }
            auto mgf_sequence{static_cast<ASN1_STRING const*>(mgf_parameters)};
            int mgf_size{ASN1_STRING_length(mgf_sequence)};
            auto mgf_start{ASN1_STRING_get0_data(mgf_sequence)}, mgf_cursor{mgf_start};
            if(mgf_size<=0 || mgf_size>128 || !mgf_start) { return false; }
            owned<X509_ALGOR,X509_ALGOR_free> mgf_hash{d2i_X509_ALGOR(nullptr,&mgf_cursor,mgf_size)};
            encoded=canonical.data();
            if(!mgf_hash || mgf_cursor!=mgf_start+mgf_size || sha2_algorithm(mgf_hash.get())!=hash_nid ||
               i2d_X509_ALGOR(mgf_hash.get(),nullptr)!=mgf_size || i2d_X509_ALGOR(mgf_hash.get(),&encoded)!=mgf_size ||
               ::std::memcmp(mgf_start,canonical.data(),static_cast<::std::size_t>(mgf_size))!=0) { return false; }
            ::std::int64_t salt{}, trailer{1};
            auto digest_type{EVP_get_digestbynid(hash_nid)};
            return digest_type && ASN1_INTEGER_get_int64(&salt,pss->saltLength)==1 && salt==EVP_MD_get_size(digest_type) &&
                (!pss->trailerField || (ASN1_INTEGER_get_int64(&trailer,pss->trailerField)==1 && trailer==1));
        }
        inline bool matching_crl_signature(X509_CRL* crl, X509_ALGOR const* outer)
        {
            // OpenSSL 3.5 has no public CRL TBS signature-algorithm getter.
            // Parse the preserved DER with the public ASN.1 sequence API;
            // never force TBS re-encoding before cryptographic verification.
            // The outer signature must describe exactly the same algorithm.
            if(!crl || !outer || X509_CRL_get_version(crl)!=1) { return false; }
            int size{i2d_X509_CRL(crl,nullptr)};
            if(size<=0 || size>4194304) { return false; }
            ::std::vector<unsigned char> der(static_cast<::std::size_t>(size)); auto output{der.data()};
            if(i2d_X509_CRL(crl,&output)!=size) { return false; }
            auto cursor{static_cast<unsigned char const*>(der.data())};
            STACK_OF(ASN1_TYPE)* outer_fields{d2i_ASN1_SEQUENCE_ANY(nullptr,&cursor,size)};
            struct fields_guard
            {
                STACK_OF(ASN1_TYPE)* value;
                ~fields_guard() { sk_ASN1_TYPE_pop_free(value,ASN1_TYPE_free); }
            } outer_guard{outer_fields};
            if(!outer_fields || cursor!=der.data()+size || sk_ASN1_TYPE_num(outer_fields)!=3) { return false; }
            auto tbs_field{sk_ASN1_TYPE_value(outer_fields,0)};
            if(!tbs_field || tbs_field->type!=V_ASN1_SEQUENCE) { return false; }
            int tbs_size{i2d_ASN1_TYPE(tbs_field,nullptr)};
            if(tbs_size<=0 || tbs_size>size) { return false; }
            ::std::vector<unsigned char> tbs(static_cast<::std::size_t>(tbs_size)); output=tbs.data();
            if(i2d_ASN1_TYPE(tbs_field,&output)!=tbs_size) { return false; }
            cursor=tbs.data();
            STACK_OF(ASN1_TYPE)* fields{d2i_ASN1_SEQUENCE_ANY(nullptr,&cursor,tbs_size)};
            fields_guard guard{fields};
            if(!fields || cursor!=tbs.data()+tbs_size || sk_ASN1_TYPE_num(fields)<3) { return false; }
            auto version{sk_ASN1_TYPE_value(fields,0)}, inner{sk_ASN1_TYPE_value(fields,1)};
            ::std::int64_t version_number{};
            if(!version || version->type!=V_ASN1_INTEGER || ASN1_INTEGER_get_int64(&version_number,version->value.integer)!=1 ||
               version_number!=1 || !inner || inner->type!=V_ASN1_SEQUENCE) { return false; }
            int inner_size{i2d_ASN1_TYPE(inner,nullptr)}, outer_size{i2d_X509_ALGOR(outer,nullptr)};
            if(inner_size<=0 || inner_size>512 || inner_size!=outer_size) { return false; }
            ::std::array<unsigned char,512> inner_der{}, outer_der{};
            auto inner_output{inner_der.data()}, outer_output{outer_der.data()};
            return i2d_ASN1_TYPE(inner,&inner_output)==inner_size && i2d_X509_ALGOR(outer,&outer_output)==outer_size &&
                ::std::memcmp(inner_der.data(),outer_der.data(),static_cast<::std::size_t>(inner_size))==0;
        }
        inline bool critical_cert(X509* cert) noexcept
        {
            for(int i{}; i<X509_get_ext_count(cert); ++i)
            {
                auto ext{X509_get_ext(cert,i)};
                if(X509_EXTENSION_get_critical(ext) && !X509_supported_extension(ext)) { return false; }
            }
            return true;
        }
        inline bool bound_expiry(ASN1_TIME const* expiry, ::std::time_t now, ::std::uint64_t& limit)
        {
            owned<ASN1_TIME,ASN1_TIME_free> current{ASN1_TIME_set(nullptr,now)};
            int days{}, seconds{};
            if(!expiry || !current || ASN1_TIME_diff(&days,&seconds,current.get(),expiry)!=1 || days<0 || seconds<0) { return false; }
            auto end{static_cast<::std::uint64_t>(now)+static_cast<::std::uint64_t>(days)*86400+static_cast<unsigned>(seconds)};
            if(end<limit) { limit=end; } return true;
        }
        inline result revocation_memory(policy const& p, ::std::string const& pin, bool record);
        inline result verify_crypto(policy const& p, manifest const& m, ::std::string_view certificate_pem,
                                    ::std::string_view crl_pem, ::std::time_t now, ::std::uint64_t& valid_until, ::std::string& signer_identity)
        {
            ::std::vector<::std::string_view> roots, certs, crls;
            if(!pem_blocks(p.root_pem,"CERTIFICATE",roots) || roots.size()>16) { return deny("CERT_001","Malformed trust anchors"); }
            if(!pem_blocks(certificate_pem,"CERTIFICATE",certs) || certs.size()>16) { return deny("CERT_002","Malformed signer chain"); }
            if(!pem_blocks(crl_pem,"X509 CRL",crls) || crls.size()>16) { return deny("CERT_005","Missing authenticated revocation evidence"); }
            owned<X509_STORE,X509_STORE_free> store{X509_STORE_new()};
            owned<X509_STORE_CTX,X509_STORE_CTX_free> ctx{X509_STORE_CTX_new()};
            if(!store || !ctx) { return deny("CERT_002","Certificate verifier unavailable"); }
            ::std::vector<owned<X509,X509_free>> root_objects, cert_objects;
            for(auto block : roots)
            {
                auto cert{pem_object<X509,d2i_X509,i2d_X509,X509_free>(block,"CERTIFICATE")};
                owned<EVP_PKEY,EVP_PKEY_free> root_key{cert ? X509_get_pubkey(cert.get()) : nullptr};
                X509_ALGOR const* root_algorithm{}; ASN1_BIT_STRING const* root_signature{};
                if(cert) { X509_get0_signature(&root_signature,&root_algorithm,cert.get()); }
                owned<BASIC_CONSTRAINTS,BASIC_CONSTRAINTS_free> root_constraints{cert ? static_cast<BASIC_CONSTRAINTS*>(X509_get_ext_d2i(cert.get(),NID_basic_constraints,nullptr,nullptr)) : nullptr};
                owned<ASN1_BIT_STRING,ASN1_BIT_STRING_free> root_usage{cert ? static_cast<ASN1_BIT_STRING*>(X509_get_ext_d2i(cert.get(),NID_key_usage,nullptr,nullptr)) : nullptr};
                if(!cert || !root_constraints || !root_constraints->ca || !root_usage || ASN1_BIT_STRING_get_bit(root_usage.get(),5)!=1 ||
                   X509_check_ca(cert.get())<=0 || !critical_cert(cert.get()) || !strong_certificate_key(cert.get(),root_key.get(),false) ||
                   !strong_signature(root_algorithm,root_signature) || X509_ALGOR_cmp(root_algorithm,X509_get0_tbs_sigalg(cert.get()))!=0 ||
                   X509_STORE_add_cert(store.get(),cert.get())!=1)
                { return deny("CERT_001","Invalid trust anchor"); }
                root_objects.push_back(::std::move(cert));
            }
            for(auto block : certs)
            {
                auto cert{pem_object<X509,d2i_X509,i2d_X509,X509_free>(block,"CERTIFICATE")};
                X509_ALGOR const* algorithm{}; ASN1_BIT_STRING const* signature{};
                if(cert) { X509_get0_signature(&signature,&algorithm,cert.get()); }
                if(!cert || !algorithm || !critical_cert(cert.get()) || X509_ALGOR_cmp(algorithm,X509_get0_tbs_sigalg(cert.get()))!=0)
                { return deny("CERT_002","Invalid certificate extensions or signature identifiers"); }
                cert_objects.push_back(::std::move(cert));
            }
            auto leaf{cert_objects.front().get()};
            owned<EVP_PKEY,EVP_PKEY_free> key{X509_get_pubkey(leaf)};
            int constraints_status{};
            owned<BASIC_CONSTRAINTS,BASIC_CONSTRAINTS_free> constraints{static_cast<BASIC_CONSTRAINTS*>(X509_get_ext_d2i(leaf,NID_basic_constraints,&constraints_status,nullptr))};
            // X509_check_ca can return zero for CA:true with a KU that omits
            // keyCertSign. Inspect basicConstraints independently as well.
            if((!constraints && constraints_status!=-1) || (constraints && constraints->ca) || X509_check_ca(leaf)!=0 || !strong_certificate_key(leaf,key.get(),true))
            { return deny("SIG_005","Signer must be a non-CA supported strong signing key"); }
            if(!m.algorithm ||
               (m.algorithm->kind==signature_kind::ecdsa && ec_curve(key.get())!=m.algorithm->curve_nid) ||
               (m.algorithm->kind==signature_kind::ed25519 && !EVP_PKEY_is_a(key.get(),"ED25519")) ||
               ((m.algorithm->kind==signature_kind::rsa_pss || m.algorithm->kind==signature_kind::rsa_pkcs1) &&
                (!EVP_PKEY_is_a(key.get(),"RSA") || EVP_PKEY_is_a(key.get(),"RSA-PSS"))))
            { return deny("SIG_005","Protected signature algorithm does not match the signer key"); }
            int critical{};
            owned<ASN1_BIT_STRING,ASN1_BIT_STRING_free> usage{static_cast<ASN1_BIT_STRING*>(X509_get_ext_d2i(leaf,NID_key_usage,&critical,nullptr))};
            owned<EXTENDED_KEY_USAGE,EXTENDED_KEY_USAGE_free> eku{static_cast<EXTENDED_KEY_USAGE*>(X509_get_ext_d2i(leaf,NID_ext_key_usage,&critical,nullptr))};
            bool code_signing{};
            if(eku) { for(int i{}; i<sk_ASN1_OBJECT_num(eku.get()); ++i) { if(OBJ_obj2nid(sk_ASN1_OBJECT_value(eku.get(),i))==NID_code_sign) { code_signing=true; } } }
            if(!usage || ASN1_BIT_STRING_get_bit(usage.get(),0)!=1 || !code_signing) { return deny("CERT_006","Explicit digitalSignature and codeSigning usage required"); }
            // Pin the certificate's SubjectPublicKeyInfo, including its actual
            // AlgorithmIdentifier parameters. Re-encoding an extracted EVP key
            // can normalize parameters and disagree with Go's raw SPKI
            // identity or an administrator's exact certificate pin.
            auto subject_public_key{X509_get_X509_PUBKEY(leaf)};
            int der_size{subject_public_key ? i2d_X509_PUBKEY(subject_public_key,nullptr) : -1};
            if(der_size<=0) { return deny("CERT_006","Invalid signer public key"); }
            ::std::vector<unsigned char> der(static_cast<::std::size_t>(der_size)); auto der_ptr{der.data()};
            digest spki{};
            if(i2d_X509_PUBKEY(subject_public_key,&der_ptr)!=der_size || !hash(der.data(),der.size(),spki)) { return deny("CERT_006","Invalid signer identity"); }
            signer_identity=hex(spki);
            rule const* authorized{};
            for(auto const& r : p.rules) { if(r.id==m.id && r.accepts(signer_identity)) { authorized=&r; break; } }
            if(!authorized) { return deny("CERT_006","Signer is not authorized for this module identity"); }
            auto remembered{revocation_memory(p,signer_identity,false)};
            if(!remembered) { return remembered; }
            if(!(authorized->algorithms & profile_bit(*m.algorithm))) { return deny("SIG_005","Signature algorithm is not explicitly authorized for this module"); }
            if(m.version<authorized->version || m.epoch<authorized->epoch) { return deny("POL_001","Module is below a protected version floor"); }
            if(m.abi!=authorized->abi || (m.capabilities & ~authorized->capabilities)) { return deny("POL_003","Manifest exceeds locally authorized capabilities"); }
            STACK_OF(X509)* untrusted{sk_X509_new_null()};
            if(!untrusted) { return deny("CERT_002","Certificate verifier unavailable"); }
            struct stack_guard { STACK_OF(X509)* ptr; ~stack_guard(){ sk_X509_free(ptr); } } chain_guard{untrusted};
            for(::std::size_t i{1}; i<cert_objects.size(); ++i) { if(!sk_X509_push(untrusted,cert_objects[i].get())) { return deny("CERT_002","Certificate verifier unavailable"); } }
            if(X509_STORE_CTX_init(ctx.get(),store.get(),leaf,untrusted)!=1) { return deny("CERT_002","Certificate verifier unavailable"); }
            auto param{X509_STORE_CTX_get0_param(ctx.get())};
            X509_VERIFY_PARAM_set_auth_level(param,2);
            X509_VERIFY_PARAM_set_depth(param,16);
            X509_VERIFY_PARAM_set_time(param,now);
            if(X509_VERIFY_PARAM_set_flags(param,X509_V_FLAG_X509_STRICT | X509_V_FLAG_CHECK_SS_SIGNATURE | X509_V_FLAG_POLICY_CHECK)!=1 || X509_verify_cert(ctx.get())!=1)
            {
                int error{X509_STORE_CTX_get_error(ctx.get())};
                return deny(error==X509_V_ERR_CERT_HAS_EXPIRED || error==X509_V_ERR_CERT_NOT_YET_VALID ? "CERT_003" : "CERT_002","Offline certificate path validation failed");
            }
            auto chain{X509_STORE_CTX_get0_chain(ctx.get())};
            for(int i{}; i<sk_X509_num(chain); ++i)
            {
                auto cert{sk_X509_value(chain,i)};
                if(!bound_expiry(X509_get0_notAfter(cert),now,valid_until)) { return deny("CERT_003","Invalid certificate expiry"); }
                owned<EVP_PKEY,EVP_PKEY_free> cert_key{X509_get_pubkey(cert)};
                X509_ALGOR const* certificate_algorithm{}; ASN1_BIT_STRING const* certificate_signature{};
                X509_get0_signature(&certificate_signature,&certificate_algorithm,cert);
                if(!strong_certificate_key(cert,cert_key.get(),i==0) || !strong_signature(certificate_algorithm,certificate_signature)) { return deny("SIG_005","Weak or unsupported certificate algorithm"); }
                if(i>0)
                {
                    int eku_status{};
                    owned<EXTENDED_KEY_USAGE,EXTENDED_KEY_USAGE_free> ca_eku{static_cast<EXTENDED_KEY_USAGE*>(X509_get_ext_d2i(cert,NID_ext_key_usage,&eku_status,nullptr))};
                    bool permits_signing{!ca_eku && eku_status==-1};
                    if(ca_eku)
                    {
                        for(int j{}; j<sk_ASN1_OBJECT_num(ca_eku.get()); ++j)
                        {
                            auto purpose{OBJ_obj2nid(sk_ASN1_OBJECT_value(ca_eku.get(),j))};
                            if(purpose==NID_code_sign || purpose==NID_anyExtendedKeyUsage) { permits_signing=true; }
                        }
                    }
                    if(!permits_signing) { return deny("CERT_006","CA extended key usage excludes code signing"); }
                }
            }
            // Authenticate the complete algorithm/purpose path before any
            // revocation result can become a durable local security fact.
            for(int i{}; i+1<sk_X509_num(chain); ++i)
            {
                auto cert{sk_X509_value(chain,i)};
                auto issuer{sk_X509_value(chain,i+1)};
                owned<EVP_PKEY,EVP_PKEY_free> issuer_key{X509_get_pubkey(issuer)};
                owned<ASN1_BIT_STRING,ASN1_BIT_STRING_free> issuer_usage{static_cast<ASN1_BIT_STRING*>(X509_get_ext_d2i(issuer,NID_key_usage,nullptr,nullptr))};
                if(!issuer_usage || ASN1_BIT_STRING_get_bit(issuer_usage.get(),6)!=1) { return deny("CERT_005","CRL issuer lacks cRLSign"); }
                bool covered{};
                for(auto block : crls)
                {
                    auto crl{pem_object<X509_CRL,d2i_X509_CRL,i2d_X509_CRL,X509_CRL_free>(block,"X509 CRL")};
                    if(!crl) { return deny("CERT_005","Malformed CRL"); }
                    if(X509_NAME_cmp(X509_CRL_get_issuer(crl.get()),X509_get_subject_name(issuer))!=0) { continue; }
                    // Profile excludes delta/indirect CRLs and unknown critical extensions.
                    if(X509_CRL_get_ext_by_NID(crl.get(),NID_delta_crl,-1)>=0 || X509_CRL_get_ext_by_NID(crl.get(),NID_issuing_distribution_point,-1)>=0)
                    { return deny("CERT_005","Delta or scoped CRLs are unsupported"); }
                    for(int j{}; j<X509_CRL_get_ext_count(crl.get()); ++j)
                    { if(X509_EXTENSION_get_critical(X509_CRL_get_ext(crl.get(),j))) { return deny("CERT_005","Unsupported critical CRL extension"); } }
                    auto revoked_entries{X509_CRL_get_REVOKED(crl.get())};
                    for(int j{}; j<sk_X509_REVOKED_num(revoked_entries); ++j)
                    {
                        auto entry{sk_X509_REVOKED_value(revoked_entries,j)};
                        for(int k{}; k<X509_REVOKED_get_ext_count(entry); ++k)
                        {
                            auto extension{X509_REVOKED_get_ext(entry,k)};
                            if(X509_EXTENSION_get_critical(extension) || OBJ_obj2nid(X509_EXTENSION_get_object(extension))==NID_certificate_issuer)
                            { return deny("CERT_005","Unsupported indirect or critical CRL entry extension"); }
                        }
                    }
                    X509_ALGOR const* crl_algorithm{}; ASN1_BIT_STRING const* crl_signature{};
                    X509_CRL_get0_signature(crl.get(),&crl_signature,&crl_algorithm);
                    if(!strong_signature(crl_algorithm,crl_signature) || !matching_crl_signature(crl.get(),crl_algorithm) ||
                       X509_CRL_verify(crl.get(),issuer_key.get())!=1)
                    { return deny("CERT_005","CRL signature verification failed"); }
                    auto last{X509_CRL_get0_lastUpdate(crl.get())}, next{X509_CRL_get0_nextUpdate(crl.get())};
                    owned<ASN1_TIME,ASN1_TIME_free> now_asn{ASN1_TIME_set(nullptr,now)};
                    int days{},seconds{};
                    if(!last || !next || !now_asn || ASN1_TIME_compare(last,now_asn.get())>0 || ASN1_TIME_compare(next,now_asn.get())<=0 ||
                       ASN1_TIME_diff(&days,&seconds,last,now_asn.get())!=1 || days<0 || seconds<0 ||
                       static_cast<::std::uint64_t>(days)*86400+static_cast<unsigned>(seconds)>p.max_age)
                    { return deny("CERT_005","CRL evidence is stale or not yet valid"); }
                    if(!bound_expiry(next,now,valid_until)) { return deny("CERT_005","Invalid CRL expiry"); }
                    auto age{static_cast<::std::uint64_t>(days)*86400+static_cast<unsigned>(seconds)};
                    auto fresh_until{static_cast<::std::uint64_t>(now)+(p.max_age-age)+1};
                    if(fresh_until<valid_until) { valid_until=fresh_until; }
                    X509_REVOKED* revoked{};
                    // OpenSSL returns 2 for removeFromCRL. This profile never
                    // automatically releases a previously held certificate.
                    if(X509_CRL_get0_by_cert(crl.get(),&revoked,cert)>0) { return deny("CERT_004","Certificate is revoked"); }
                    // Any fresh issuer CRL revoking this certificate wins over another
                    // supplied older 'good' CRL; process every applicable CRL.
                    covered=true;
                }
                if(!covered) { return deny("CERT_005","CRL evidence does not cover every non-root certificate"); }
            }
            ::std::vector<unsigned char> signature;
            auto const& algorithm{*m.algorithm};
            if(algorithm.kind==signature_kind::ecdsa)
            {
                int width{static_cast<int>(algorithm.component_bytes)};
                owned<BIGNUM,BN_free> r{BN_bin2bn(reinterpret_cast<unsigned char const*>(m.signature.data()),width,nullptr)};
                owned<BIGNUM,BN_free> s{BN_bin2bn(reinterpret_cast<unsigned char const*>(m.signature.data()+width),width,nullptr)};
                BIGNUM* raw_order{};
                if(EVP_PKEY_get_bn_param(key.get(),OSSL_PKEY_PARAM_EC_ORDER,&raw_order)!=1 || !raw_order) { return deny("SIG_005","ECDSA group order unavailable"); }
                owned<BIGNUM,BN_free> order{raw_order}, half{BN_dup(raw_order)};
                owned<ECDSA_SIG,ECDSA_SIG_free> sig{ECDSA_SIG_new()};
                if(!r || !s || !order || !half || !sig || BN_rshift1(half.get(),order.get())!=1 ||
                   BN_is_zero(r.get()) || BN_is_zero(s.get()) || BN_cmp(r.get(),order.get())>=0 || BN_cmp(s.get(),half.get())>0)
                { return deny("SIG_003","Invalid or non-canonical low-S ECDSA signature"); }
                if(ECDSA_SIG_set0(sig.get(),r.get(),s.get())!=1) { return deny("SIG_003","Invalid ECDSA signature"); }
                r.release(); s.release();
                int size{i2d_ECDSA_SIG(sig.get(),nullptr)};
                if(size<=0) { return deny("SIG_003","Invalid ECDSA signature"); }
                signature.resize(static_cast<::std::size_t>(size)); auto cursor{signature.data()};
                if(i2d_ECDSA_SIG(sig.get(),&cursor)!=size) { return deny("SIG_003","Invalid ECDSA signature"); }
            }
            else
            {
                if(algorithm.kind==signature_kind::ed25519 &&
                   !admitted_ed25519_signature(reinterpret_cast<unsigned char const*>(m.signature.data()),m.signature.size()))
                { return deny("SIG_003","Noncanonical or small-order Ed25519 signature"); }
                if((algorithm.kind==signature_kind::rsa_pss || algorithm.kind==signature_kind::rsa_pkcs1) &&
                   m.signature.size()!=static_cast<::std::size_t>(EVP_PKEY_get_size(key.get())))
                { return deny("SIG_003","RSA signature length does not match the modulus"); }
                signature.assign(m.signature.begin(),m.signature.end());
            }
            // The algorithm identifier itself is authenticated. Never rebuild
            // it from an unsigned suggestion or retry a different algorithm.
            ::std::string structure{"\x84\x6a" "Signature1",12};
            append_head(structure,2,m.protected_headers.size()); structure.append(m.protected_headers); structure+='\x40';
            append_head(structure,2,m.payload.size()); structure.append(m.payload);
            auto digest_type{algorithm.hash_nid ? EVP_get_digestbynid(algorithm.hash_nid) : nullptr};
            owned<EVP_MD_CTX,EVP_MD_CTX_free> verifier{EVP_MD_CTX_new()}; EVP_PKEY_CTX* parameters{};
            if(!verifier || EVP_DigestVerifyInit(verifier.get(),&parameters,digest_type,nullptr,key.get())!=1)
            { return deny("SIG_005","Selected signature algorithm is unavailable"); }
            if(algorithm.kind==signature_kind::rsa_pss || algorithm.kind==signature_kind::rsa_pkcs1)
            {
                int padding{algorithm.kind==signature_kind::rsa_pss ? RSA_PKCS1_PSS_PADDING : RSA_PKCS1_PADDING};
                if(!parameters || EVP_PKEY_CTX_set_rsa_padding(parameters,padding)<=0 ||
                   (algorithm.kind==signature_kind::rsa_pss && (!digest_type ||
                    EVP_PKEY_CTX_set_rsa_mgf1_md(parameters,digest_type)<=0 ||
                    EVP_PKEY_CTX_set_rsa_pss_saltlen(parameters,EVP_MD_get_size(digest_type))<=0)))
                { return deny("SIG_005","Required RSA padding/hash/salt profile is unavailable"); }
            }
            if(EVP_DigestVerify(verifier.get(),signature.data(),signature.size(),reinterpret_cast<unsigned char const*>(structure.data()),structure.size())!=1)
            { return deny("SIG_003","COSE signature verification failed"); }
            return {};
        }

        // A previously authenticated revocation is a protected security fact.
        // Never let a downloader undo it by replaying an older, still-fresh
        // issuer CRL. This strict profile retires the release key permanently;
        // administrator recovery provisions a new key and policy version.
        inline result revocation_memory(policy const& p, ::std::string const& pin, bool record)
        {
            auto dir{protected_open(p.state_path,true)}; struct stat st{};
            if(!dir || ::fstat(dir.value,&st)!=0 || st.st_uid!=::geteuid() || (st.st_mode&0777)!=0700)
            { return deny("POL_001","Cannot authenticate protected revocation storage"); }
            fd lock;
            if(record)
            {
                lock=fd{::openat(dir.value,".lock",O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK,0600)};
                if(!lock || ::fstat(lock.value,&st)!=0 || !S_ISREG(st.st_mode) || st.st_uid!=::geteuid() ||
                   (st.st_mode&0777)!=0600 || st.st_nlink!=1 || ::flock(lock.value,LOCK_EX)!=0)
                { return deny("POL_001","Cannot lock protected revocation state"); }
            }
            ::std::string name{"revoked-"}; name+=pin;
            if(record)
            {
                fd created{::openat(dir.value,name.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK,0600)};
                if(created)
                {
                    ::std::string content{"UWVM-WSEA-REVOKED-1\n"}; content+=pin; content+='\n';
                    ::std::size_t done{};
                    while(done<content.size())
                    {
                        auto n{::write(created.value,content.data()+done,content.size()-done)};
                        if(n<0 && errno==EINTR) { continue; }
                        if(n<=0) { return deny("POL_001","Cannot persist authenticated revocation"); }
                        done+=static_cast<::std::size_t>(n);
                    }
                    if(::fsync(created.value)!=0 || ::fsync(dir.value)!=0)
                    { return deny("POL_001","Cannot establish authenticated revocation durability"); }
                }
                else if(errno!=EEXIST) { return deny("POL_001","Cannot persist authenticated revocation"); }
            }
            fd marker{::openat(dir.value,name.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK)};
            if(!marker)
            {
                if(!record && errno==ENOENT) { return {}; }
                return deny("POL_001","Unsafe protected revocation record");
            }
            if(::fstat(marker.value,&st)!=0 || !S_ISREG(st.st_mode) || st.st_uid!=::geteuid() || (st.st_mode&0777)!=0600 || st.st_nlink!=1)
            { return deny("POL_001","Unsafe protected revocation record"); }
            // A partial record remains a denial, including after a crash. Its
            // contents cannot grant permission or clear the protected fact.
            return deny("CERT_004","Release key has a persistent authenticated revocation record");
        }
        struct floor { ::std::string id; ::std::uint64_t version{},epoch{}; };
        inline result commit_state(policy const& p, manifest const& m, ::std::time_t checked_at, ::std::uint64_t valid_until)
        {
            auto dir{protected_open(p.state_path,true)}; struct stat st{};
            if(!dir || ::fstat(dir.value,&st)!=0 || st.st_uid!=::geteuid() || (st.st_mode&0777)!=0700)
            { return deny("POL_001","Rollback directory must be preinitialized, private, and owned by the execution user"); }
            fd lock{::openat(dir.value,".lock",O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK,0600)};
            if(!lock || ::fstat(lock.value,&st)!=0 || !S_ISREG(st.st_mode) || st.st_uid!=::geteuid() || (st.st_mode&0777)!=0600 || st.st_nlink!=1 || ::flock(lock.value,LOCK_EX)!=0)
            { return deny("POL_001","Cannot lock protected rollback state"); }
            for(auto const& r : p.rules)
            {
                if(r.id!=m.id) { continue; }
                if(m.signers.empty())
                {
                    if(r.threshold>1) { return deny("POL_002","Missing authenticated signature quorum"); }
                    auto remembered{revocation_memory(p,r.signer,false)};
                    if(!remembered) { return remembered; }
                }
                else
                {
                    if(m.signers.size()<r.threshold) { return deny("POL_002","Insufficient authenticated signature quorum"); }
                    for(auto const& pin : m.signers)
                    {
                        if(!r.accepts(pin)) { return deny("CERT_006","Authenticated signer is not authorized"); }
                        auto remembered{revocation_memory(p,pin,false)};
                        if(!remembered) { return remembered; }
                    }
                }
                break;
            }
            fd state{::openat(dir.value,"state",O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK)};
            ::std::string text;
            if(!state || ::fstat(state.value,&st)!=0 || !S_ISREG(st.st_mode) || st.st_uid!=::geteuid() || (st.st_mode&0777)!=0600 || st.st_nlink!=1 || !read_fd(state.value,1048576,text))
            { return deny("POL_001","Protected rollback state is missing or unsafe"); }
            auto lines{split(text,'\n')};
            if(lines.size()<3 || lines.front()!="UWVM-WSEA-STATE-1" || !lines.back().empty()) { return deny("POL_001","Malformed protected rollback state"); }
            auto header{split(lines[1],' ')}; ::std::uint64_t policy_version{},last_time{};
            if(header.size()!=3 || !number(header[0],policy_version) || !number(header[1],last_time) ||
               !(hex_digest(header[2]) || (policy_version==0 && last_time==0 && header[2]=="-"))) { return deny("POL_001","Malformed protected rollback state"); }
            if(policy_version==0 && text!="UWVM-WSEA-STATE-1\n0 0 -\n") { return deny("POL_001","Malformed bootstrap rollback state"); }
            if(p.version<policy_version || (p.version==policy_version && p.fingerprint!=header[2])) { return deny("POL_002","Policy rollback or same-version replacement rejected"); }
            // A slow verifier can finish after a later request has committed.
            // Sample the real clock while holding the state lock; an earlier
            // request's timestamp is a lower bound, not the commit timestamp.
            // Never clamp to last_time: a real rollback must still be denied.
            auto now{::std::time(nullptr)};
            if(now<1577836800 || now<checked_at || static_cast<::std::uint64_t>(now)>=valid_until)
            { return deny("POL_004","Trusted time rolled back during verification or authenticated evidence expired"); }
            if(static_cast<::std::uint64_t>(now)<last_time) { return deny("POL_004","Protected clock rollback detected"); }
            ::std::vector<floor> floors;
            bool found{};
            for(::std::size_t i{2}; i+1<lines.size(); ++i)
            {
                auto fields{split(lines[i],'|')}; floor f;
                if(fields.size()!=3 || !identifier(fields[0]) || !number(fields[1],f.version) || f.version==0 || !number(fields[2],f.epoch) || floors.size()>=1024)
                { return deny("POL_001","Malformed module rollback floor"); }
                f.id=fields[0];
                for(auto const& old : floors) { if(old.id==f.id) { return deny("POL_001","Duplicate rollback floor"); } }
                if(f.id==m.id)
                {
                    if(m.version<f.version || m.epoch<f.epoch) { return deny("POL_001","Module rollback rejected"); }
                    f.version=m.version; f.epoch=m.epoch; found=true;
                }
                floors.push_back(::std::move(f));
            }
            if(!found) { if(floors.size()>=1024) { return deny("POL_001","Rollback state capacity exceeded"); } floors.push_back({::std::string{m.id},m.version,m.epoch}); }
            ::std::string updated{"UWVM-WSEA-STATE-1\n"};
            updated+=::std::to_string(p.version); updated+=' '; updated+=::std::to_string(now); updated+=' '; updated+=p.fingerprint; updated+='\n';
            for(auto const& f : floors) { updated+=f.id; updated+='|'; updated+=::std::to_string(f.version); updated+='|'; updated+=::std::to_string(f.epoch); updated+='\n'; }
            if(updated==text) { return {}; } // Same-second recheck needs no second durable write.
            unsigned char random[16]{};
            if(RAND_bytes(random,sizeof random)!=1) { return deny("POL_001","Cannot create atomic rollback transaction"); }
            ::std::string name{".state-"};
            constexpr char chars[]{"0123456789abcdef"};
            for(auto b : random) { name+=chars[b>>4]; name+=chars[b&15]; }
            fd tmp{::openat(dir.value,name.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600)};
            if(!tmp) { return deny("POL_001","Cannot create atomic rollback transaction"); }
            ::std::size_t written{}; bool ok{true};
            while(written<updated.size())
            {
                auto n{::write(tmp.value,updated.data()+written,updated.size()-written)};
                if(n<0 && errno==EINTR) { continue; }
                if(n<=0) { ok=false; break; }
                written+=static_cast<::std::size_t>(n);
            }
            if(!ok || ::fsync(tmp.value)!=0 || ::renameat(dir.value,name.c_str(),dir.value,"state")!=0)
            { ::unlinkat(dir.value,name.c_str(),0); return deny("POL_001","Rollback state transaction failed"); }
            if(::fsync(dir.value)!=0) { return deny("POL_001","Rollback state durability could not be established"); }
            return {};
        }
    }

    class verified_module
    {
        ::std::byte const* bytes_{};
        ::std::size_t size_{};
        detail::digest digest_{};
        detail::digest manifest_digest_{};
        ::std::string algorithm_{}; // owned profile names, never borrowed evidence
        ::std::vector<::std::string> signers_;
        ::std::uint64_t capabilities_{};
        ::std::uint64_t version_{}, epoch_{}, verified_at_{}, valid_until_{};
        ::std::string id_;
        ::std::string path_, expected_id_;
        ::std::shared_ptr<detail::policy const> policy_;
        verified_module(::std::byte const* bytes, ::std::size_t size, detail::digest digest, detail::digest manifest_digest,
                        ::std::string algorithm, ::std::uint64_t caps, ::std::string path, ::std::string expected_id,
                        ::std::string id, ::std::uint64_t version, ::std::uint64_t epoch, ::std::uint64_t now, ::std::uint64_t valid_until,
                        ::std::shared_ptr<detail::policy const> policy, ::std::vector<::std::string> signers) noexcept
            : bytes_{bytes},size_{size},digest_{digest},manifest_digest_{manifest_digest},algorithm_{::std::move(algorithm)},signers_{::std::move(signers)},capabilities_{caps},
              version_{version},epoch_{epoch},verified_at_{now},valid_until_{valid_until},id_{::std::move(id)},
              path_{::std::move(path)},expected_id_{::std::move(expected_id)},policy_{::std::move(policy)} {}
        friend result verify_with_policy(::std::shared_ptr<detail::policy const>,::std::string_view,::std::byte const*,::std::size_t,::std::string_view);
    public:
        verified_module(verified_module const&) = delete;
        verified_module& operator=(verified_module const&) = delete;
        [[nodiscard]] bool matches(::std::byte const* bytes, ::std::size_t size) const noexcept;
        [[nodiscard]] ::std::uint64_t capabilities() const noexcept { return capabilities_; }
        [[nodiscard]] ::std::string_view algorithm() const noexcept { return algorithm_; }
    };
    inline result verify_with_policy(::std::shared_ptr<detail::policy const> p,::std::string_view path,::std::byte const* bytes,::std::size_t size,::std::string_view expected_id = {})
    {
        if(!p) { return deny("POL_002","No trusted security policy configured for this module"); }
        if(!detail::path_text(path) || !bytes || size<8 || size>p->max_size) { return deny("SIG_004","Module exceeds the signed execution profile"); }
        constexpr unsigned char wasm_header[]{0,97,115,109,1,0,0,0};
        if(::std::memcmp(bytes,wasm_header,sizeof wasm_header)!=0) { return deny("VM_001","Secure execution requires a version-1 WebAssembly module"); }
        ::std::string_view image{reinterpret_cast<char const*>(bytes),size};
        detail::embedded_evidence evidence;
        auto extracted{detail::extract_embedded(image,evidence)};
        if(!extracted) { return extracted; }
        detail::manifest m;
        if(!detail::decode(evidence.cose,m)) { return deny("SIG_002","Unsupported or malformed embedded WSEA v2 COSE_Sign1"); }
        if(!expected_id.empty() && m.id!=expected_id) { return deny("CERT_006","Signed identity does not match the explicitly selected module identity"); }
        detail::digest d{}, complete{};
        if(m.size!=evidence.prefix.size() || !detail::hash_images(image,evidence.prefix.size(),d,complete) || CRYPTO_memcmp(d.data(),m.hash.data(),d.size())!=0)
        { return deny("SIG_004","Complete WASM prefix SHA-256 does not match the embedded signature"); }
        auto now{::std::time(nullptr)};
        if(now<1577836800 || static_cast<::std::uint64_t>(now)<m.before || static_cast<::std::uint64_t>(now)>=m.after)
        { return deny("POL_004","Trusted OS time is unavailable or outside the manifest validity interval"); }
        auto valid_until{m.after};
        ::std::string algorithms;
        for(auto const& entry : evidence.signatures)
        {
            detail::manifest contribution;
            if(!detail::decode(entry.cose,contribution) || contribution.payload!=m.payload)
            { return deny("SIG_002","Signatures must authenticate the identical manifest"); }
            ::std::string signer;
            auto crypto{detail::verify_crypto(*p,contribution,entry.certificates,entry.crls,now,valid_until,signer)};
            if(!crypto)
            {
                if(!signer.empty() && ::std::strcmp(crypto.code,"CERT_004")==0)
                { return detail::revocation_memory(*p,signer,true); }
                return crypto;
            }
            bool seen{};
            for(auto const& prior : m.signers) { if(prior==signer) { seen=true; break; } }
            if(!seen) { m.signers.push_back(::std::move(signer)); }
            if(!algorithms.empty()) { algorithms+='+'; }
            algorithms+=contribution.algorithm->name;
        }
        detail::rule const* module_rule{};
        for(auto const& r : p->rules) { if(r.id==m.id) { module_rule=&r; break; } }
        if(!module_rule || m.signers.size()<module_rule->threshold)
        { return deny("POL_002","Distinct valid signatures do not satisfy the protected threshold"); }
        auto state{detail::commit_state(*p,m,now,valid_until)};
        if(!state) { return state; }
        detail::digest manifest_digest{};
        // Bind the protected algorithm and exact signature as well as payload.
        // Re-signing unchanged payload under another allowed profile requires
        // a fresh receipt, just like replacing any other authenticated evidence.
        if(!detail::hash(evidence.payload.data(),evidence.payload.size(),manifest_digest)) { return deny("SIG_005","SHA-256 unavailable"); }
        return {nullptr,nullptr,::std::shared_ptr<verified_module const>{new verified_module{bytes,size,complete,manifest_digest,::std::move(algorithms),m.capabilities,
            detail::normalize(path),::std::string{expected_id},::std::string{m.id},m.version,m.epoch,static_cast<::std::uint64_t>(now),valid_until,::std::move(p),::std::move(m.signers)}}};
    }
    inline bool verified_module::matches(::std::byte const* bytes, ::std::size_t size) const noexcept
    {
        detail::digest actual{};
        if(bytes!=bytes_ || size!=size_ || !policy_ || !detail::hash(bytes,size,actual) || CRYPTO_memcmp(actual.data(),digest_.data(),actual.size())!=0) { return false; }
        ::std::shared_ptr<detail::policy const> current;
        // Receipts cannot outlive policy changes, evidence expiry or revocation.
        // New loads read the new policy; an old receipt requires a fresh load.
        if(!detail::load_policy(policy_->config_path,current) || current->fingerprint!=policy_->fingerprint) { return false; }
        auto now{::std::time(nullptr)};
        if(now<0 || static_cast<::std::uint64_t>(now)<verified_at_ || static_cast<::std::uint64_t>(now)>=valid_until_) { return false; }
        detail::manifest admitted; admitted.id=id_; admitted.version=version_; admitted.epoch=epoch_;
        admitted.signers=signers_;
        return static_cast<bool>(detail::commit_state(*current,admitted,now,valid_until_));
    }
    inline result configure_policy(::std::string_view path)
    {
        ::std::lock_guard lock{detail::registry_mutex};
        if(detail::default_policy) { return deny("POL_002","Security policy is already frozen"); }
        return detail::load_policy(path,detail::default_policy);
    }
    inline result select_module(::std::string_view module_path,::std::string_view policy_path,::std::string_view expected_id = {})
    {
        ::std::lock_guard lock{detail::registry_mutex};
        auto module{detail::normalize(module_path)};
        if(module.empty() || detail::selections.size()>=1024) { return deny("POL_002","Invalid secure module selection"); }
        for(auto const& selected : detail::selections) { if(selected.module==module) { return deny("POL_002","Duplicate secure module selection"); } }
        ::std::shared_ptr<detail::policy const> policy;
        for(auto const& selected : detail::selections) { if(selected.policy_path==policy_path) { policy=selected.config; break; } }
        if(!policy) { auto checked{detail::load_policy(policy_path,policy)}; if(!checked) { return checked; } }
        if(expected_id.empty())
        {
            if(policy->rules.size()!=1) { return deny("POL_002","Shared policies require --secure named-module <wasm> <policy> <expected-id>"); }
            expected_id=policy->rules.front().id;
        }
        bool authorized{};
        for(auto const& r : policy->rules) { if(r.id==expected_id) { authorized=true; break; } }
        if(!authorized) { return deny("CERT_006","Selected module identity has no local publisher rule"); }
        detail::selections.push_back({::std::move(module),::std::string{policy_path},::std::string{expected_id},::std::move(policy)}); return {};
    }
    inline bool module_selected(::std::string_view path)
    {
        auto name{detail::normalize(path)}; ::std::lock_guard lock{detail::registry_mutex};
        for(auto const& selected : detail::selections) { if(name==selected.module) { return true; } }
        return false;
    }
    inline bool secure_enabled() noexcept
    { ::std::lock_guard lock{detail::registry_mutex}; return !detail::selections.empty() || static_cast<bool>(detail::default_policy); }
    inline char const* status_message() noexcept
    { return secure_enabled() ? "WSEA: explicit module security enabled; offline COSE / algorithm allowlist / X.509 / CRL / rollback protection" : "WSEA: disabled; select each module with --secure module <wasm> <policy>"; }
    inline ::std::string status_details()
    {
        ::std::lock_guard lock{detail::registry_mutex};
        ::std::string out;
        for(auto const& selected : detail::selections)
        {
            out+="\nmodule="; out+=selected.module;
            out+="\n  expected_id="; out+=selected.expected_id;
            out+="\n  policy="; out+=selected.policy_path;
            ::std::shared_ptr<detail::policy const> current;
            auto checked{detail::load_policy(selected.policy_path,current)};
            if(checked)
            {
                out+="\n  policy_version="; out+=::std::to_string(current->version);
                for(auto const& rule : current->rules)
                {
                    if(rule.id!=selected.expected_id) { continue; }
                    out+="\n  allowed_algorithms="; bool first{true};
                    for(auto const& profile : detail::signature_profiles)
                    { if(rule.algorithms & detail::profile_bit(profile)) { if(!first) { out+=','; } out+=profile.name; first=false; } }
                }
            }
            else { out+="\n  policy_error="; out+=checked.code; }
            out+="\n  profile=COSE_Sign1/explicit-algorithms/full-file-SHA256/X.509/CRL";
        }
        return out;
    }
    inline result verify_module(::std::string_view path,::std::byte const* bytes,::std::size_t size)
    {
        ::std::shared_ptr<detail::policy const> p; ::std::string expected_id;
        {
            auto name{detail::normalize(path)}; ::std::lock_guard lock{detail::registry_mutex};
            for(auto const& selected : detail::selections) { if(name==selected.module) { p=selected.config; expected_id=selected.expected_id; break; } }
            if(!p) { p=detail::default_policy; }
        }
        if(!p) { return deny("POL_002","No trusted security policy configured for this module"); }
        ::std::shared_ptr<detail::policy const> current;
        auto loaded{detail::load_policy(p->config_path,current)};
        return loaded ? verify_with_policy(::std::move(current),path,bytes,size,expected_id) : loaded;
    }
    inline result verify_module(::std::string_view policy_path,::std::string_view path,::std::byte const* bytes,::std::size_t size)
    {
        ::std::shared_ptr<detail::policy const> p; auto configured{detail::load_policy(policy_path,p)};
        return configured ? verify_with_policy(::std::move(p),path,bytes,size) : configured;
    }
#else
    class verified_module
    {
    public:
        [[nodiscard]] bool matches(::std::byte const*,::std::size_t) const noexcept { return false; }
        [[nodiscard]] ::std::uint64_t capabilities() const noexcept { return 0; }
        [[nodiscard]] ::std::string_view algorithm() const noexcept { return {}; }
    };
    inline result configure_policy(::std::string_view) noexcept { return deny("SIG_005","This build has no supported OpenSSL 3 / protected offline state backend"); }
    inline result select_module(::std::string_view,::std::string_view,::std::string_view = {}) noexcept { return configure_policy({}); }
    inline bool module_selected(::std::string_view) noexcept { return false; }
    inline bool secure_enabled() noexcept { return false; }
    inline char const* status_message() noexcept { return "WSEA: disabled; this build does not support secure verification"; }
    inline ::std::string status_details() { return {}; }
    inline result verify_module(::std::string_view,::std::byte const*,::std::size_t) noexcept { return configure_policy({}); }
    inline result verify_module(::std::string_view,::std::string_view,::std::byte const*,::std::size_t) noexcept { return configure_policy({}); }
#endif
}
