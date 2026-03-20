cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "/usr/local/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()


# Add OpenMP configuration
if(APPLE)
    set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp")
    set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
    set(OpenMP_C_LIB_NAMES "omp")
    set(OpenMP_CXX_LIB_NAMES "omp")
    set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")
    set(OpenMP_C_INCLUDE_DIR "/opt/homebrew/opt/libomp/include")
    set(OpenMP_CXX_INCLUDE_DIR "/opt/homebrew/opt/libomp/include")
    
endif()


project (main VERSION 1.0)

# Set compiler and linker flags for macOS
if(APPLE)
    # Basic compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

find_package(SEAL CONFIG REQUIRED)
find_package(OpenMP REQUIRED)

add_executable(main pdte.cpp server.cpp client.cpp utils.cpp tree_utils.cpp parser.cpp)
add_executable(cmp_bench cmp_bench.cpp server.cpp utils.cpp tree_utils.cpp parser.cpp)

target_link_libraries(main SEAL::seal OpenMP::OpenMP_CXX)
target_link_libraries(cmp_bench SEAL::seal OpenMP::OpenMP_CXX)


