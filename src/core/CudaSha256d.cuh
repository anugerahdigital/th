#pragma once
#include <stdint.h>
#include <cuda_runtime.h>

__device__ __forceinline__ static uint32_t ROTR(uint32_t x, uint32_t n){
    return (x >> n) | (x << (32 - n));
}
__device__ __forceinline__ static uint32_t Ch (uint32_t x,uint32_t y,uint32_t z){ return (x & y) ^ (~x & z); }
__device__ __forceinline__ static uint32_t Maj(uint32_t x,uint32_t y,uint32_t z){ return (x & y) ^ (x & z) ^ (y & z); }
__device__ __forceinline__ static uint32_t BSIG0(uint32_t x){ return ROTR(x,2)  ^ ROTR(x,13) ^ ROTR(x,22); }
__device__ __forceinline__ static uint32_t BSIG1(uint32_t x){ return ROTR(x,6)  ^ ROTR(x,11) ^ ROTR(x,25); }
__device__ __forceinline__ static uint32_t SSIG0(uint32_t x){ return ROTR(x,7)  ^ ROTR(x,18) ^ (x >> 3); }
__device__ __forceinline__ static uint32_t SSIG1(uint32_t x){ return ROTR(x,17) ^ ROTR(x,19) ^ (x >> 10); }

// ВАЖНО: только декларация, с extern — без инициализатора!
extern __constant__ uint32_t K[64];

__device__ __forceinline__ void sha256_compress(const uint32_t W_in[16], uint32_t H[8]) {
    uint32_t W[64];
    #pragma unroll
    for (int i=0;i<16;i++) W[i]=W_in[i];
    #pragma unroll
    for (int i=16;i<64;i++) W[i] = SSIG0(W[i-15]) + W[i-16] + SSIG1(W[i-2]) + W[i-7];

    uint32_t a=H[0],b=H[1],c=H[2],d=H[3],e=H[4],f=H[5],g=H[6],h=H[7];
    #pragma unroll
    for (int i=0;i<64;i++){
        uint32_t t1 = h + BSIG1(e) + Ch(e,f,g) + K[i] + W[i];
        uint32_t t2 = BSIG0(a) + Maj(a,b,c);
        h=g; g=f; f=e; e=d + t1;
        d=c; c=b; b=a; a=t1 + t2;
    }
    H[0]+=a; H[1]+=b; H[2]+=c; H[3]+=d; H[4]+=e; H[5]+=f; H[6]+=g; H[7]+=h;
}

// double SHA256 для 80‑байтового заголовка (LE)
__device__ __forceinline__ void sha256d_header80(const uint32_t hdr_le[20], uint32_t out_be[8]) {
    uint32_t H0[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };

    uint32_t W0[16], W1[16];
    #pragma unroll
    for (int i=0;i<16;i++){
        uint32_t v = hdr_le[i];
        W0[i] = __byte_perm(v, 0, 0x0123); // LE->BE
    }

    uint32_t H1[8];
    #pragma unroll
    for (int i=0;i<8;i++) H1[i]=H0[i];
    sha256_compress(W0, H1);

    #pragma unroll
    for (int i=0;i<16;i++) W1[i]=0;

    #pragma unroll
    for (int i=0;i<4;i++){
        uint32_t v = hdr_le[16+i];
        W1[i] = __byte_perm(v, 0, 0x0123); // LE->BE
    }
    W1[4]  = 0x80000000u;
    W1[15] = 640; // 80 байт * 8

    sha256_compress(W1, H1);

    uint32_t W2[16] = {0};
    #pragma unroll
    for (int i=0;i<8;i++) W2[i]=H1[i];
    W2[8]  = 0x80000000u;
    W2[15] = 256; // 32 байта * 8

    uint32_t H2[8];
    #pragma unroll
    for (int i=0;i<8;i++) H2[i]=H0[i];
    sha256_compress(W2, H2);

    #pragma unroll
    for (int i=0;i<8;i++) out_be[i]=H2[i];
}
