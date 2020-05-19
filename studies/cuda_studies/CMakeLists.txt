cmake_minimum_required(VERSION 3.8)
set(CMAKE_CUDA_COMPILER /usr/local/cuda/bin/nvcc)

project(cuda_studies CXX CUDA)

#include_directories(include ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})

#add_library(awkward_array_scan
#        src/awkward_offset_arrays.cu
#        src/awkward_offset_arrays.cpp)


add_executable(${PROJECT_NAME}_1 src/study_1.cu)

set_target_properties(${PROJECT_NAME}_1 PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
target_compile_features(${PROJECT_NAME}_1 PUBLIC cxx_std_11)
#target_link_libraries(scan PRIVATE awkward_array_scan)