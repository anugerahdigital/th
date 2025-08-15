#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace util {

// hex <-> bytes
std::vector<uint8_t> hex_to_bytes(std::string_view hex);
std::string bytes_to_hex(const uint8_t* data, size_t len, bool lowercase = true);
inline std::string bytes_to_hex(const std::vector<uint8_t>& v, bool lowercase = true) {
    return bytes_to_hex(v.data(), v.size(), lowercase);
}
void reverse_inplace(std::vector<uint8_t>& v);

// sha256 / sha256d
void sha256(const uint8_t* data, size_t len, uint8_t out32[32]);
void sha256d(const uint8_t* data, size_t len, uint8_t out32[32]);

// удобные обёртки
std::string sha256d_hex_le(const std::vector<uint8_t>& bytes); // hex LE
std::string sha256d_hex_be(const std::vector<uint8_t>& bytes); // hex BE

// меркл-рут из ветки (если ветка пустая — это просто txid)
std::string merkle_root_from_branch_le(
    const std::string& leaf_txid_hex_le,
    const std::vector<std::string>& branch_hex_le // каждый элемент hex (LE)
);

// --- BTC-компакт в таргет, сборка заголовка ---
std::vector<uint8_t> compact_to_target_be(uint32_t nbits); // 32 байта BE
bool hash_meets_target_be(const uint8_t hash_be[32], const std::vector<uint8_t>& target_be);

// собрать 80-байтный заголовок (в правильной эндийности)
std::vector<uint8_t> build_block_header80_le(
    const std::string& version_hex_le,
    const std::string& prevhash_hex_le,
    const std::string& merkle_root_hex_le,
    const std::string& ntime_hex_be, // из notify
    const std::string& nbits_hex_be, // из notify
    uint32_t nonce_le                 // уже LE-значение
);

// helpers конвертации кратких hex к uint32 и обратно (BE<->LE представления)
uint32_t hex_be_to_u32(std::string_view hex8); // "689e3f29" -> 0x689e3f29
std::string u32_to_hex_le(uint32_t v);         // 0x12345678 -> "78563412"

} // namespace util
