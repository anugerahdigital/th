#pragma once
#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include "MinerApp.hpp"
#include "util/Util.hpp"
#include "stratum/StratumClient.hpp"

namespace tht {

class CpuMiner {
public:
    CpuMiner(StratumClient& stratum, const std::string& user, int threads = 1)
    : stratum_(stratum), user_(user), threads_(threads > 0 ? threads : 1) {}

    // запуск нового джоба
    void start(const Job& job,
               const std::string& extranonce1,
               int extranonce2_size);

    // остановка текущего джоба
    void stop();

private:
    void worker_loop(int lane); // lane ∈ [0..threads_-1]

private:
    StratumClient& stratum_;
    std::string user_;
    int threads_ = 1;

    // текущее состояние работы
    Job job_;
    std::string en1_;
    int en2_size_ = 0;

    // общий extranonce2 для всех потоков (фиксируем на жизнь джоба)
    std::string en2_hex_;

    std::atomic<bool> stopping_{false};
    std::vector<std::thread> thrs_;

    // счётчики для хешрейта
    std::atomic<uint64_t> checked_sum_{0};
    std::atomic<uint64_t> last_print_ms_{0};
};

} // namespace tht
