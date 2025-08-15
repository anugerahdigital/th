# Библиотека с CUDA-обвязкой (без PoW-ядра; только self-test и инфо)
add_library(cuda_core STATIC
    core/CudaSolver.cu
    core/CudaSolver.hpp
)
target_include_directories(cuda_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(cuda_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CUDA_SEPARABLE_COMPILATION ON
)
target_compile_options(cuda_core PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:--use_fast_math>
    $<$<COMPILE_LANGUAGE:CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter>
)
