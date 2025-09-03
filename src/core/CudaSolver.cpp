#include "CudaSolver.hpp"
#include <cstdio>

using namespace tht;

void CudaSolver::self_test() {
    std::puts("[CUDA] self-test skipped (built without CUDA)");
}

void CudaSolver::gpu_mine_once_and_submit(const Job&,
                                          const std::string&,
                                          const std::vector<std::string>&,
                                          const std::string&,
                                          const std::string&,
                                          StratumClient&,
                                          double) {
    std::puts("[CUDA] gpu_mine_once_and_submit not available in CPU-only build");
}

void CudaSolver::probe_devices() {}
void CudaSolver::init() {}
void CudaSolver::run_dummy_kernel() {}
