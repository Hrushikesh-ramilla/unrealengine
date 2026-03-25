cmake_minimum_required(VERSION 3.20)
project(NullRing
    VERSION 0.1.0
    DESCRIPTION "High-performance financial risk analytics engine"
    LANGUAGES CXX
)

# â”€â”€ C++20 Standard â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# â”€â”€ Cross-Platform Compiler Configuration â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
if (MSVC)
    # MSVC:
    #   /W4       â€“ High warning level (MSVC equivalent of -Wall -Wextra)
    #   /WX       â€“ Treat warnings as errors
    #   /O2       â€“ Maximize speed
    #   /arch:AVX2 â€“ Enable AVX2 SIMD instructions
    #   /MT       â€“ Static CRT linkage (no vcredist dependency)
    #   /EHsc     â€“ Standard C++ exception handling (required by STL headers)
    #   /wd4324   â€“ Suppress "structure was padded due to alignment specifier"
    #              (intentional: we use alignas(64) for cache-line isolation)
    #   /DNDEBUG  â€“ Disable assertions in release
    add_compile_options(/W4 /WX /O2 /arch:AVX2 /MT /EHsc /wd4324 /DNDEBUG)
else()
    # GCC / Clang: strict warnings + treat as errors
    add_compile_options(-Wall -Wextra -Werror)
endif()

# â”€â”€ Include Paths â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
include_directories(${PROJECT_SOURCE_DIR}/include)

# â”€â”€ Export compile_commands.json for tooling â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# â”€â”€ Source Files (placeholder) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# add_executable(nullring src/main.cpp)

# â”€â”€ Tests â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
enable_testing()

add_executable(test_structures tests/test_structures.cpp)
target_include_directories(test_structures PRIVATE ${PROJECT_SOURCE_DIR}/include)

if (NOT MSVC)
    target_link_libraries(test_structures PRIVATE pthread)
endif()

add_test(NAME DataStructureTests COMMAND test_structures)

# â”€â”€ Benchmarks â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
add_executable(latency_bench benchmarks/latency_bench.cpp)
target_include_directories(latency_bench PRIVATE ${PROJECT_SOURCE_DIR}/include)

if (MSVC)
    # HFT Optimization Flags (MSVC):
    # /O2:       Maximize speed
    # /arch:AVX2: Use AVX2 SIMD instructions
    # /GL:       Whole-program optimization (MSVC equivalent of -flto)
    # /DNDEBUG:  Disable assertions
    target_compile_options(latency_bench PRIVATE /O2 /arch:AVX2 /GL /DNDEBUG)
else()
    # HFT Optimization Flags (GCC/Clang):
    # -O3:             Max optimization
    # -march=native:   Use local CPU architecture instructions (AVX2/AVX-512)
    # -flto:           Link-time optimization across files
    # -fno-exceptions: Remove exception handling overhead from the hot path
    # -DNDEBUG:        Disable assertions
    target_compile_options(latency_bench PRIVATE -O3 -march=native -flto -fno-exceptions -DNDEBUG)
    target_link_libraries(latency_bench PRIVATE pthread)
endif()
