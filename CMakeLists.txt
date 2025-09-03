cmake_minimum_required(VERSION 3.24)
project(tht_miner LANGUAGES CXX)

# Optional CUDA support
option(USE_CUDA "Build with CUDA support" ON)
if(USE_CUDA)
  find_package(CUDAToolkit QUIET)
  if(CUDAToolkit_FOUND)
    enable_language(CUDA)
  else()
    message(WARNING "CUDA toolkit not found; building CPU-only")
    set(USE_CUDA OFF)
  endif()
endif()

if(EXISTS "/usr/bin/gcc-12" AND EXISTS "/usr/bin/g++-12")
  set(CMAKE_C_COMPILER /usr/bin/gcc-12)
  set(CMAKE_CXX_COMPILER /usr/bin/g++-12)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(USE_CUDA)
  set(CMAKE_CUDA_STANDARD 17)
  set(CMAKE_CUDA_STANDARD_REQUIRED ON)
  if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 86)
  endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_subdirectory(src)
