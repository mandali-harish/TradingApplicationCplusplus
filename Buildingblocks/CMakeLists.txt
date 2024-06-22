cmake_minimum_required(VERSION 3.0)

project(LowLatencyApp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_FLAGS "-std=c++2a -Wall -Wextra -Werror -Wpedantic")
set(CMAKE_VERBOSE_MAKEFILE on)

file(GLOB SOURCES "*.cpp")

include_directories(${PROJECT_SOURCE_DIR})

add_library(libcommon STATIC ${SOURCES})

list(APPEND LIBS libcommon)
list(APPEND LIBS pthread)

add_executable(thread_example thread_example.cpp)
target_link_libraries(thread_example PUBLIC ${LIBS})