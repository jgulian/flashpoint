cmake_minimum_required(VERSION 3.22)
project(flashpoint)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MACOSX_RPATH 1)

set(ABSL_DEFAULT_LINKOPTS "")

add_library(flashpoint_lib)

include(FetchContent)

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Threads)

FetchContent_Declare(
        cli11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11
        GIT_TAG v2.2.0
)

FetchContent_MakeAvailable(cli11)

set(PROTO_FILES
        protos/kv.proto
        protos/raft.proto
        protos/log.proto
        )

add_library(proto ${PROTO_FILES})
target_link_libraries(proto
        PUBLIC
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::grpc++
        )
target_include_directories(proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
protobuf_generate(TARGET proto LANGUAGE cpp)
protobuf_generate(TARGET proto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")

target_link_libraries(flashpoint_lib proto CLI11::CLI11)

# APPLICATION
add_executable(flashpoint)
add_subdirectory(src)

target_link_libraries(flashpoint flashpoint_lib)

# TESTING

enable_testing()

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        release-1.11.0
)
FetchContent_MakeAvailable(googletest)

add_executable(flashpoint_test)
add_subdirectory(test)
target_link_libraries(flashpoint_test PUBLIC flashpoint_lib gmock)

include(GoogleTest)
gtest_discover_tests(flashpoint_test)


add_executable(playground main.cpp)