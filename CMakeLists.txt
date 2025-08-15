cmake_minimum_required(VERSION 3.24)
project(tht_miner_cuda LANGUAGES CXX CUDA)

# (Опционально закрепим компиляторы — ты уже сделал это в build.sh)
if(NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER /usr/bin/gcc-12)
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER /usr/bin/g++-12)
endif()
if(NOT DEFINED CMAKE_CUDA_HOST_COMPILER)
  set(CMAKE_CUDA_HOST_COMPILER /usr/bin/g++-12)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Куда складывать артефакты
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Архитектуры CUDA (подбери под свою карту)
# 86 = Ampere (RTX 30), 89 = Ada (RTX 40)
# Если не уверен — оставь 86; при желании установим через -DCMAKE_CUDA_ARCHITECTURES
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
  set(CMAKE_CUDA_ARCHITECTURES 86)
endif()

add_subdirectory(src)

