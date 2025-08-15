#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "common/Job.hpp"        // твой Job

namespace tht {

class StratumClient;             // forward

struct GpuInfo {
    int device_id = 0;
    std::string name;
    size_t total_mem_bytes = 0;
    int sm_count = 0;
    int major = 0, minor = 0;
};

class CudaSolver {
public:
    void set_device(int id) { device_id_ = id; }
    void set_worker_count(int n) { workers_ = n > 0 ? n : 1; }

    void self_test();

    // запуск короткого gpu-скана с авто-сабмитом
    void gpu_mine_once_and_submit(const Job& job,
                                  const std::string& coinbase_txid_le,
                                  const std::vector<std::string>& merkle_branch_le,
                                  const std::string& extranonce2_hex,
                                  const std::string& ntime_hex,
                                  StratumClient& client,
                                  double current_share_diff);   // <-- добавили diff

private:
    void probe_devices();
    void init();
    void run_dummy_kernel();

private:
    int device_id_ = 0;
    int workers_ = 1;
    bool inited_ = false;
    std::vector<GpuInfo> devices_;
};

} // namespace tht
