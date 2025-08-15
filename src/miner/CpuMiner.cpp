#include "CpuMiner.hpp"
#include <random>
#include <cstdio>
#include <chrono>

using namespace tht;

static std::vector<std::string> parse_merkle_array_le(const std::string& arr) {
    std::vector<std::string> out;
    auto lb = arr.find('[');
    auto rb = arr.rfind(']');
    if (lb == std::string::npos || rb == std::string::npos || rb <= lb) return out;
    size_t i = lb + 1;
    bool in = false, esc = false;
    std::string cur;
    while (i < rb) {
        char c = arr[i++];
        if (in) {
            if (esc) { esc = false; cur.push_back(c); continue; }
            if (c == '\\') { esc = true; continue; }
            if (c == '"') { out.push_back(cur); cur.clear(); in = false; continue; }
            cur.push_back(c);
        } else {
            if (c == '"') { in = true; continue; }
        }
    }
    return out;
}

void CpuMiner::start(const Job& job, const std::string& extranonce1, int extranonce2_size) {
    stop();
    job_ = job;
    en1_ = extranonce1;
    en2_size_ = extranonce2_size;
    checked_sum_.store(0);
    last_print_ms_.store(0);

    if (en1_.empty() || en2_size_ <= 0) {
        std::printf("[CPU] missing extranonce info; not starting\n");
        return;
    }

    // сгенерим один общепоточный extranonce2
    std::mt19937_64 rng{std::random_device{}()};
    uint64_t r = rng();
    std::vector<uint8_t> en2_bytes(en2_size_, 0);
    for (int i = 0; i < en2_size_; ++i) en2_bytes[i] = uint8_t((r >> (8*i)) & 0xFF);
    en2_hex_ = util::bytes_to_hex(en2_bytes);

    stopping_.store(false);
    thrs_.clear();
    thrs_.reserve(threads_);
    for (int lane = 0; lane < threads_; ++lane) {
        thrs_.emplace_back(&CpuMiner::worker_loop, this, lane);
    }
}

void CpuMiner::stop() {
    stopping_.store(true);
    for (auto& t : thrs_) if (t.joinable()) t.join();
    thrs_.clear();
}

void CpuMiner::worker_loop(int lane) {
    #ifdef __linux__
    cpu_set_t set; CPU_ZERO(&set);
    CPU_SET(lane % std::thread::hardware_concurrency(), &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    #endif
    // подготовим константы работы
    std::string coinbase_hex = job_.coinbase1 + en1_ + en2_hex_ + job_.coinbase2;
    auto coinbase_bytes = util::hex_to_bytes(coinbase_hex);
    auto coinbase_txid_le = util::sha256d_hex_le(coinbase_bytes);

    auto branch_le = parse_merkle_array_le(job_.merkle);
    std::string merkle_root_le = util::merkle_root_from_branch_le(coinbase_txid_le, branch_le);

    uint32_t nbits_u32 = util::hex_be_to_u32(job_.nbits);
    auto target_be = util::compact_to_target_be(nbits_u32);

    std::mt19937_64 rng{std::random_device{}()};
    uint32_t nonce = uint32_t(rng()) + uint32_t(lane); // стартовая точка смещена lane
    const uint32_t stride = uint32_t(threads_) * 8;

    uint64_t checked_local = 0;
    auto last = std::chrono::steady_clock::now();

    while (!stopping_.load()) {
        auto header = util::build_block_header80_le(
            job_.version, job_.prevhash, merkle_root_le, job_.ntime, job_.nbits, nonce);

        uint8_t h[32];
        util::sha256d(header.data(), header.size(), h); // BE

        if (util::hash_meets_target_be(h, target_be)) {
            char nonce_hex[9]; std::snprintf(nonce_hex, sizeof(nonce_hex), "%08x", nonce);
            bool ok = stratum_.send_submit(user_, job_.job_id, en2_hex_, job_.ntime, nonce_hex);
            std::printf("[CPU] SHARE FOUND (lane=%d): nonce=%s -> submit (%s)\n",
                        lane, nonce_hex, ok ? "sent" : "send-failed");
        }

        nonce += stride;
        checked_local++;

        // агрегация и печать хешрейта раз в ~1с из каждого воркера (но безопасно)
        auto now = std::chrono::steady_clock::now();
        if (now - last >= std::chrono::seconds(1)) {
            checked_sum_.fetch_add(checked_local);
            checked_local = 0;
            last = now;

            // один из потоков будет печатать (примерно раз в секунду)
            uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            uint64_t prev = last_print_ms_.exchange(ms);
            if (prev == 0 || ms - prev >= 900) {
                double mhps = (double)checked_sum_.exchange(0) / 1e6;
                std::printf("[CPU] rate: ~%.2f MH/s (%d threads)\n", mhps, threads_);
            }
        }
    }
}
