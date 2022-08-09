cmake_minimum_required(VERSION 3.22)
project(flashpoint)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MACOSX_RPATH 1)

set(ABSL_PROPAGATE_CXX_STD ON)
SET(WITH_PROTOC)

add_library(flashpoint_lib)

include(FetchContent)

find_package(Threads)

link_libraries(ws2_32 wsock32)

FetchContent_Declare(
        cli11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11
        GIT_TAG v2.2.0
)

FetchContent_Declare(
        protobuf
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
        GIT_TAG v21.4
        SOURCE_SUBDIR cmake
)

FetchContent_Declare(
        gRPC
        GIT_REPOSITORY https://github.com/grpc/grpc
        GIT_TAG v1.48.0
)

FetchContent_MakeAvailable(cli11 gRPC)

# message(STATUS thgus ${CMAKE_PREFIX_PATH})
# add_subdirectory(${CMAKE_PREFIX_PATH}/_deps/protobuf/cmake _protobuf)
find_package(Protobuf MODULE)

set(PROTO_FILES
        protos/kv.proto
        protos/raft.proto
        protos/log.proto
        protos/test.proto
        )

add_library(proto ${PROTO_FILES})
target_link_libraries(proto
        PUBLIC
        protobuf::libprotobuf
        grpc++
        )
target_include_directories(proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# find_program(GRPC_CPP_PLUGIN_EXE protoc REQUIRED)
# find_program(GRPC_CPP_PLUGIN_EXE grpc_cpp_plugin REQUIRED)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

#find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
message(STATUS "thing ${_GRPC_CPP_PLUGIN_EXECUTABLE}")

protobuf_generate(TARGET proto LANGUAGE cpp)

#get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
# message(STATUS "Found gRPC C++ plugin at ${grpc_cpp_plugin_location}")

message(STATUS "grpc cpp plugin ${grpc_cpp_plugin}")
protobuf_generate(TARGET proto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${_GRPC_CPP_PLUGIN_EXECUTABLE}")

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
target_link_libraries(playground PUBLIC flashpoint_lib)