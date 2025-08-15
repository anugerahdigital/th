#include <cuda_runtime.h>
#include "CudaSha256d.cuh"

__constant__ uint32_t K[64] = {
  0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
  0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
  0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
  0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
  0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
  0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
  0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
  0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

// Структура входа: 20 слов 32‑бит LE, nonce — это слово #19
struct HeaderLE { uint32_t w[20]; };

// Буфер результатов: найденные nonce и их количество
struct FoundBuf {
    uint32_t *nonces;
    uint32_t *count;
    uint32_t capacity;
};

__device__ __forceinline__ bool hash_meets_target_be(const uint32_t h[8], const uint32_t* target_be){
    // сравниваем как 32‑байт BE массивы: h < target?
    #pragma unroll
    for (int i=0;i<8;i++){
        uint32_t a = h[i];
        uint32_t b = target_be[i];
        if (a < b) return true;
        if (a > b) return false;
    }
    return true; // равенство тоже ок
}

extern "C" __global__
void kernel_scan_nonce(HeaderLE base, uint32_t start, uint32_t stride,
                       const uint32_t* target_be, FoundBuf fb)
{
    uint32_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t nonce = start + gid;

    HeaderLE hdr = base;
    hdr.w[19] = nonce; // подставляем nonce (LE)

    uint32_t h[8];
    sha256d_header80(hdr.w, h);

    if (hash_meets_target_be(h, target_be)) {
        // запишем nonce
        uint32_t idx = atomicAdd(fb.count, 1u);
        if (idx < fb.capacity) fb.nonces[idx] = nonce;
    }
}
