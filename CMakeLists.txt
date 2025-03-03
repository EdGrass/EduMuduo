cmake_minimum_required(VERSION 3.10)
project(EduModuo)

find_package(fmt REQUIRED)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_library(EduModuo STATIC ${SOURCES})

target_link_libraries(EduModuo PRIVATE fmt::fmt)

add_executable(test_server src/test.cpp)
target_link_libraries(test_server PRIVATE EduModuo fmt::fmt)