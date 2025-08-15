#include "CudaSolver.hpp"
#include <cuda_runtime.h>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <array>
#include <chrono>
#include <string>

// наши заголовки
#include "CudaSha256d.cuh"
#include "util/Util.hpp"
#include "stratum/StratumClient.hpp"

using namespace tht;

// ===== прототип ядра из CudaKernel.cu =====
struct HeaderLE { uint32_t w[20]; };
struct FoundBuf { uint32_t* nonces; uint32_t* count; uint32_t capacity; };

extern "C" __global__
void kernel_scan_nonce(HeaderLE base, uint32_t start, uint32_t stride,
                       const uint32_t* target_be, FoundBuf fb);

// ===== утилиты =====
static void checkCuda(cudaError_t err, const char* ctx) {
    if (err != cudaSuccess) {
        char buf[256];
        snprintf(buf, sizeof(buf), "CUDA error at %s: %s", ctx, cudaGetErrorString(err));
        throw std::runtime_error(buf);
    }
}

static inline uint64_t now_ms() {
    return (uint64_t) std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ===== простой self-test =====
__global__ void dummy_kernel(unsigned long long* out) {
    if (threadIdx.x == 0 && blockIdx.x == 0) {
        *out = (unsigned long long)(gridDim.x) * (unsigned long long)(blockDim.x);
    }
}

void CudaSolver::probe_devices() {
    devices_.clear();
    int count = 0;
    checkCuda(cudaGetDeviceCount(&count), "cudaGetDeviceCount");
    for (int i = 0; i < count; ++i) {
        cudaDeviceProp p{};
        checkCuda(cudaGetDeviceProperties(&p, i), "cudaGetDeviceProperties");
        GpuInfo gi;
        gi.device_id = i;
        gi.name = p.name;
        gi.total_mem_bytes = p.totalGlobalMem;
        gi.sm_count = p.multiProcessorCount;
        gi.major = p.major;
        gi.minor = p.minor;
        devices_.push_back(gi);
    }
}

void CudaSolver::init() {
    if (inited_) return;
    probe_devices();
    if (devices_.empty()) {
        throw std::runtime_error("GPU devices not found. Is NVIDIA driver available inside WSL?");
    }
    checkCuda(cudaSetDevice(device_id_), "cudaSetDevice");
    inited_ = true;
}

void CudaSolver::run_dummy_kernel() {
    unsigned long long* d_out = nullptr;
    unsigned long long h_out = 0;
    checkCuda(cudaMalloc(&d_out, sizeof(unsigned long long)), "cudaMalloc");
    dim3 grid(32);
    dim3 block(128);
    dummy_kernel<<<grid, block>>>(d_out);
    checkCuda(cudaGetLastError(), "kernel launch");
    checkCuda(cudaMemcpy(&h_out, d_out, sizeof(unsigned long long), cudaMemcpyDeviceToHost), "cudaMemcpy");
    cudaFree(d_out);
    printf("[CUDA] dummy kernel OK: grid=%u block=%u -> wrote=%llu\n",
           grid.x, block.x, h_out);
}

void CudaSolver::self_test() {
    init();
    printf("=== CUDA Devices ===\n");
    for (const auto& d : devices_) {
        double gb = static_cast<double>(d.total_mem_bytes) / (1024.0*1024.0*1024.0);
        printf("GPU %d: %s | SMs=%d | CC=%d.%d | VRAM=%.2f GiB\n",
               d.device_id, d.name.c_str(), d.sm_count, d.major, d.minor, gb);
    }
    printf("====================\n");
    run_dummy_kernel();
}

// ===== DIFF → share-target (BE, 256 бит) =====
// DIFF1 (Bitcoin diff=1) как 256‑бит BE
static const uint32_t DIFF1_BE[8] = {
    0x00000000u, 0xFFFF0000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u
};

// делаем out_be ≈ DIFF1_BE / diff.
// Реализация через умножение на m=round(1/diff) с 256‑бит переносами и насыщением.
static void make_share_target_from_diff(double diff, uint32_t out_be[8]) {
    if (diff <= 0.0) diff = 1.0;
    long double inv = 1.0L / (long double)diff;
    uint64_t m = (uint64_t)(inv + 0.5L);
    unsigned __int128 carry = 0;
    for (int i = 7; i >= 0; --i) {
        unsigned __int128 v = (unsigned __int128)DIFF1_BE[i] * (unsigned __int128)m + carry;
        out_be[i] = (uint32_t)(v & 0xFFFFFFFFu);
        carry = v >> 32;
    }
    if (carry) {
        for (int i = 0; i < 8; ++i) out_be[i] = 0xFFFFFFFFu;
    }
}

// ===== Вспомог.: собрать 80 байт заголовка в 20 слов LE =====
static HeaderLE build_header20_le(const Job& job,
                                  const std::string& merkle_root_le_hex,
                                  const std::string& ntime_hex,
                                  const std::string& nbits_hex,
                                  uint32_t nonce_le)
{
    std::vector<uint8_t> header = util::build_block_header80_le(
        job.version, job.prevhash, merkle_root_le_hex, ntime_hex, nbits_hex, nonce_le);

    if (header.size() != 80) {
        throw std::runtime_error("header size != 80");
    }

    HeaderLE out{};
    for (int i = 0; i < 20; ++i) {
        uint32_t w = 0;
        w |= (uint32_t)header[i*4 + 0] << 0;
        w |= (uint32_t)header[i*4 + 1] << 8;
        w |= (uint32_t)header[i*4 + 2] << 16;
        w |= (uint32_t)header[i*4 + 3] << 24;
        out.w[i] = w;
    }
    return out;
}

// ===== Один батч сканирования на GPU, возвращает найденные nonces =====
static uint32_t gpu_scan_batch(const HeaderLE& base_hdr20_le,
                               const std::array<uint32_t,8>& target_be,
                               uint32_t start_nonce,
                               uint32_t threads_per_block,
                               uint32_t blocks,
                               uint32_t max_found,
                               std::vector<uint32_t>& out_nonces)
{
    // d_target
    uint32_t* d_target = nullptr;
    checkCuda(cudaMalloc(&d_target, sizeof(uint32_t)*8), "cudaMalloc target");
    checkCuda(cudaMemcpy(d_target, target_be.data(), sizeof(uint32_t)*8, cudaMemcpyHostToDevice),
              "memcpy target");

    // d_nonces + d_count
    uint32_t* d_nonces = nullptr;
    uint32_t* d_count  = nullptr;
    checkCuda(cudaMalloc(&d_nonces, sizeof(uint32_t) * max_found), "cudaMalloc nonces");
    checkCuda(cudaMalloc(&d_count,  sizeof(uint32_t)), "cudaMalloc count");
    checkCuda(cudaMemset(d_count, 0, sizeof(uint32_t)), "memset count");

    HeaderLE base_dev = base_hdr20_le;
    FoundBuf fb_dev{ d_nonces, d_count, max_found };

    const uint32_t stride = threads_per_block * blocks;
    dim3 grid(blocks);
    dim3 block(threads_per_block);

    kernel_scan_nonce<<<grid, block>>>(base_dev, start_nonce, stride, d_target, fb_dev);
    checkCuda(cudaGetLastError(), "kernel_scan_nonce launch");
    checkCuda(cudaDeviceSynchronize(), "kernel_scan_nonce sync");

    uint32_t h_count = 0;
    checkCuda(cudaMemcpy(&h_count, d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost), "copy count");
    if (h_count > max_found) h_count = max_found;

    out_nonces.resize(h_count);
    if (h_count) {
        checkCuda(cudaMemcpy(out_nonces.data(), d_nonces, sizeof(uint32_t)*h_count, cudaMemcpyDeviceToHost),
                  "copy nonces");
    }

    cudaFree(d_target);
    cudaFree(d_nonces);
    cudaFree(d_count);
    return h_count;
}

// ====== ГЛАВНОЕ: GPU-майнинг по share-target + авто-сабмит ======
void CudaSolver::gpu_mine_once_and_submit(const Job& job,
                                          const std::string& coinbase_txid_le,
                                          const std::vector<std::string>& merkle_branch_le,
                                          const std::string& extranonce2_hex,
                                          const std::string& ntime_hex,
                                          StratumClient& client,
                                          double current_share_diff)
{
    init();

    // 1) merkle root (LE). Если ветка пустая — это будет сам coinbase txid (LE).
    std::string merkle_root_le = util::merkle_root_from_branch_le(coinbase_txid_le, merkle_branch_le);

    // 2) share-target из diff (BE, 8 слов)
    std::array<uint32_t,8> target_be{};
    make_share_target_from_diff(current_share_diff, target_be.data());

    // 3) базовый заголовок (nonce подставляет ядро)
    HeaderLE base_hdr = build_header20_le(job, merkle_root_le, ntime_hex, job.nbits, /*nonce=*/0);

    // 4) параметры батча
    const uint32_t threads_per_block = 256;
    const uint32_t blocks            = 256;            // 65 536 нитей/батч
    const uint32_t stride            = threads_per_block * blocks;
    const uint32_t found_cap         = 1024;

    uint64_t t0 = now_ms();
    uint32_t start_nonce = 0;
    uint64_t total_checked = 0;

    // несколько батчей подряд (например, 16)
    for (int batch = 0; batch < 16; ++batch) {
        std::vector<uint32_t> nonces;
        uint32_t found = gpu_scan_batch(base_hdr, target_be, start_nonce,
                                        threads_per_block, blocks, found_cap, nonces);

        total_checked += stride;
        uint64_t t1 = now_ms();
        double dt = (double)(t1 - t0) / 1000.0;
        double mhps = dt > 0 ? (double)total_checked / 1e6 / dt : 0.0;

        std::printf("[GPU] batch=%d checked=%u total=%.0f K, rate=%.2f MH/s, found=%u\n",
                    batch, stride, total_checked/1000.0, mhps, found);

        // 5) авто‑сабмит найденных шар через StratumClient
        for (uint32_t nonce_le : nonces) {
            char nonce_hex[9]; std::snprintf(nonce_hex, sizeof(nonce_hex), "%08x", nonce_le);
            bool ok = client.submit_share(extranonce2_hex, ntime_hex, std::string(nonce_hex));
            std::printf("[GPU] SUBMIT nonce=%s -> %s\n", nonce_hex, ok ? "sent" : "send-failed");
        }

        start_nonce += stride;
    }
}
