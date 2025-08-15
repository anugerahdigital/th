#include "Util.hpp"
#include <openssl/sha.h>
#include <stdexcept>
#include <cctype>
#include <algorithm>

namespace util {

static uint8_t from_hex(char c) {
    if (c >= '0' && c <= '9') return uint8_t(c - '0');
    if (c >= 'a' && c <= 'f') return uint8_t(10 + c - 'a');
    if (c >= 'A' && c <= 'F') return uint8_t(10 + c - 'A');
    throw std::runtime_error("bad hex char");
}

std::vector<uint8_t> hex_to_bytes(std::string_view hex) {
    if (hex.size() % 2 != 0) throw std::runtime_error("hex length must be even");
    std::vector<uint8_t> out;
    out.reserve(hex.size()/2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        uint8_t hi = from_hex(hex[i]);
        uint8_t lo = from_hex(hex[i+1]);
        out.push_back((hi << 4) | lo);
    }
    return out;
}

std::string bytes_to_hex(const uint8_t* data, size_t len, bool lowercase) {
    static const char* lc = "0123456789abcdef";
    static const char* uc = "0123456789ABCDEF";
    const char* tab = lowercase ? lc : uc;
    std::string s;
    s.resize(len*2);
    for (size_t i = 0; i < len; ++i) {
        s[2*i+0] = tab[(data[i] >> 4) & 0xF];
        s[2*i+1] = tab[(data[i]     ) & 0xF];
    }
    return s;
}

void reverse_inplace(std::vector<uint8_t>& v) {
    std::reverse(v.begin(), v.end());
}

void sha256(const uint8_t* data, size_t len, uint8_t out32[32]) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data, len);
    SHA256_Final(out32, &ctx);
}

void sha256d(const uint8_t* data, size_t len, uint8_t out32[32]) {
    uint8_t tmp[32];
    sha256(data, len, tmp);
    sha256(tmp, 32, out32);
}

std::string sha256d_hex_le(const std::vector<uint8_t>& bytes) {
    uint8_t h[32];
    sha256d(bytes.data(), bytes.size(), h);
    std::vector<uint8_t> v(h, h+32);
    reverse_inplace(v); // LE-печать как txid
    return bytes_to_hex(v);
}

std::string sha256d_hex_be(const std::vector<uint8_t>& bytes) {
    uint8_t h[32];
    sha256d(bytes.data(), bytes.size(), h);
    return bytes_to_hex(h, 32);
}

static std::vector<uint8_t> hex_le_to_bytes(std::string_view hex_le) {
    return hex_to_bytes(hex_le); // уже в LE-порядке
}

static std::string bytes_to_hex_le(const std::vector<uint8_t>& b) {
    return bytes_to_hex(b);
}

std::string merkle_root_from_branch_le(
    const std::string& leaf_txid_hex_le,
    const std::vector<std::string>& branch_hex_le
) {
    // leaf/branch даны как LE hex (txid формат). Для dSHA256 оперируем BE-байтами.
    auto root = hex_le_to_bytes(leaf_txid_hex_le);
    // LE->BE
    reverse_inplace(root);
    for (const auto& sib_le : branch_hex_le) {
        auto sib = hex_le_to_bytes(sib_le);
        reverse_inplace(sib); // LE->BE
        std::vector<uint8_t> cat;
        cat.reserve(root.size() + sib.size());
        cat.insert(cat.end(), root.begin(), root.end());
        cat.insert(cat.end(), sib.begin(), sib.end());
        uint8_t h[32];
        sha256d(cat.data(), cat.size(), h);
        root.assign(h, h+32); // BE
    }
    reverse_inplace(root); // -> LE
    return bytes_to_hex(root);
}

// --- compact nbits -> target (BE 32 bytes)
std::vector<uint8_t> compact_to_target_be(uint32_t nbits) {
    uint32_t exp  = (nbits >> 24) & 0xFF;
    uint32_t mant = nbits & 0x007FFFFF;
    bool neg = (nbits & 0x00800000) != 0;
    (void)neg;
    std::vector<uint8_t> target(32, 0);
    // mant << (8*(exp-3))
    int shift = (int)exp - 3;
    if (shift < 0) shift = 0;
    // поместим mant в BE
    uint8_t m[3] = {
        uint8_t((mant >> 16) & 0xFF),
        uint8_t((mant >> 8) & 0xFF),
        uint8_t((mant) & 0xFF)
    };
    int idx = 32 - (shift + 3); // позиция старшего байта mant в 32-байтном BE буфере
    if (idx < 0) idx = 0;
    if (idx <= 29) {
        target[idx+0] = m[0];
        target[idx+1] = m[1];
        target[idx+2] = m[2];
    }
    return target;
}

bool hash_meets_target_be(const uint8_t hash_be[32], const std::vector<uint8_t>& target_be) {
    // валиден, если hash_be <= target_be (как числа BE)
    for (int i = 0; i < 32; ++i) {
        if (hash_be[i] < target_be[i]) return true;
        if (hash_be[i] > target_be[i]) return false;
    }
    return true; // равны
}

uint32_t hex_be_to_u32(std::string_view s) {
    if (s.size() != 8) throw std::runtime_error("hex8 expected");
    auto b = hex_to_bytes(s);
    return (uint32_t(b[0])<<24) | (uint32_t(b[1])<<16) | (uint32_t(b[2])<<8) | uint32_t(b[3]);
}

std::string u32_to_hex_le(uint32_t v) {
    uint8_t b[4] = {
        uint8_t(v & 0xFF),
        uint8_t((v >> 8) & 0xFF),
        uint8_t((v >> 16) & 0xFF),
        uint8_t((v >> 24) & 0xFF)
    };
    return bytes_to_hex(b, 4);
}

std::vector<uint8_t> build_block_header80_le(
    const std::string& version_hex_le,
    const std::string& prevhash_hex_le,
    const std::string& merkle_root_hex_le,
    const std::string& ntime_hex_be,
    const std::string& nbits_hex_be,
    uint32_t nonce_le
) {
    auto ver   = hex_to_bytes(version_hex_le);     // уже LE
    auto prev  = hex_to_bytes(prevhash_hex_le);    // LE
    auto mrkl  = hex_to_bytes(merkle_root_hex_le); // LE
    auto ntime_be = hex_to_bytes(ntime_hex_be);
    auto nbits_be = hex_to_bytes(nbits_hex_be);

    // ntime/nbits нужно переложить в LE
    std::vector<uint8_t> ntime = ntime_be; reverse_inplace(ntime);
    std::vector<uint8_t> nbits = nbits_be; reverse_inplace(nbits);

    uint8_t nonce[4] = {
        uint8_t(nonce_le & 0xFF),
        uint8_t((nonce_le >> 8) & 0xFF),
        uint8_t((nonce_le >> 16) & 0xFF),
        uint8_t((nonce_le >> 24) & 0xFF)
    };

    std::vector<uint8_t> hdr;
    hdr.reserve(80);
    hdr.insert(hdr.end(), ver.begin(), ver.end());
    hdr.insert(hdr.end(), prev.begin(), prev.end());
    hdr.insert(hdr.end(), mrkl.begin(), mrkl.end());
    hdr.insert(hdr.end(), ntime.begin(), ntime.end());
    hdr.insert(hdr.end(), nbits.begin(), nbits.end());
    hdr.insert(hdr.end(), nonce, nonce+4);
    return hdr;
}

} // namespace util
