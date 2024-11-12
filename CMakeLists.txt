cmake_minimum_required(VERSION 3.23)
project(AbstractTransferFunctions)

# C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find the LLVM package
find_package(LLVM REQUIRED CONFIG)

# Set up LLVM
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(LLVMConfig)

# Add the LLVM headers and libraries
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Define the executable
add_executable(AbstractTransferFunctions main.cpp)

# Link the LLVM libraries
target_link_libraries(AbstractTransferFunctions LLVM)
