#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "common/Job.hpp"          // <-- ДОБАВЛЕНО
#include "core/CudaSolver.hpp"
#include "stratum/StratumClient.hpp"

namespace tht {

struct MinerConfig {
    int device_id = 0;
    int cpu_threads = 0;
    int workers = 1;
    bool selftest = false;
    std::string url;
    std::string user;
    std::string pass;
};

// struct Job  <-- УДАЛИ
// (теперь берём из common/Job.hpp)

class CpuMiner; // forward

class MinerApp {
public:
    int run(const MinerConfig& cfg);

private:
    // парсеры/утилиты
    bool parse_subscribe_result(const std::string& line, std::string& extranonce1, int& extranonce2_size);
    bool extract_notify_params(const std::string& line, std::vector<std::string>& out_params);
    static void trim(std::string& s);
    static std::string strip_quotes(const std::string& s);
    void pretty_print_job(const Job& j) const;

private:
    CudaSolver solver_;
    StratumClient stratum_;

    // state
    std::string extranonce1_;
    int extranonce2_size_ = -1;
    double current_diff_ = 0.0;
    Job current_job_;

    // cpu miner
    CpuMiner* cpuMiner_ = nullptr;
};

} // namespace tht
