#pragma once
#include <string>

namespace tht {

struct Job {
    std::string job_id;
    std::string prevhash;
    std::string coinbase1;
    std::string coinbase2;
    std::string merkle;   // строка с JSON-массивом, как приходит от пула
    std::string version;
    std::string nbits;
    std::string ntime;
    std::string clean;
};

} // namespace tht
