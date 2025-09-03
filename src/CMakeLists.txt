add_library(cuda_core STATIC)

target_include_directories(cuda_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(USE_CUDA)
    target_sources(cuda_core PRIVATE
        core/CudaSolver.cu
        core/CudaKernel.cu
        core/CudaSha256d.cuh
        core/CudaSolver.hpp)
    set_target_properties(cuda_core PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CUDA_SEPARABLE_COMPILATION ON)
    target_compile_options(cuda_core PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--use_fast_math>
        $<$<COMPILE_LANGUAGE:CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter>)
else()
    target_sources(cuda_core PRIVATE
        core/CudaSolver.cpp
        core/CudaSolver.hpp)
    target_compile_options(cuda_core PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter>)
endif()

add_library(util STATIC
    util/Util.cpp
    util/Util.hpp)

target_include_directories(util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(stratum STATIC
    stratum/StratumClient.cpp
    stratum/StratumClient.hpp)

target_include_directories(stratum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(stratum PUBLIC util)

add_library(miner STATIC
    miner/CpuMiner.cpp
    miner/CpuMiner.hpp
    miner/MinerApp.cpp
    miner/MinerApp.hpp)

target_include_directories(miner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(miner PUBLIC stratum util cuda_core OpenSSL::Crypto Threads::Threads)

add_executable(thtminer
    cli/main.cpp)

target_link_libraries(thtminer PRIVATE miner)
