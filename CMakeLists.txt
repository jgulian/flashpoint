cmake_minimum_required(VERSION 3.22)
project(flashpoint)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MACOSX_RPATH 1)

set(ABSL_PROPAGATE_CXX_STD ON)


IF (WIN32)
    link_libraries(ws2_32 wsock32)
ELSE ()
    # set stuff for other systems
ENDIF ()

# Dependencies
include(FetchContent)
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
FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp
        GIT_TAG yaml-cpp-0.7.0
)
FetchContent_MakeAvailable(cli11 gRPC yaml-cpp)

find_package(Protobuf MODULE)


# Protos library
set(PROTO_FILES
        protos/kv.proto
        protos/raft.proto
        protos/log.proto
        )

add_library(proto ${PROTO_FILES})
target_link_libraries(proto PUBLIC protobuf::libprotobuf grpc++)
target_include_directories(proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
protobuf_generate(TARGET proto LANGUAGE cpp)
protobuf_generate(TARGET proto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${_GRPC_CPP_PLUGIN_EXECUTABLE}")


# LIBRARY
add_library(flashpoint_lib)
target_link_libraries(flashpoint_lib PUBLIC proto CLI11::CLI11 yaml-cpp::yaml-cpp)


# APPLICATION
add_executable(flashpoint)
add_subdirectory(src)

target_link_libraries(flashpoint PRIVATE flashpoint_lib)


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
target_link_libraries(flashpoint_test PRIVATE flashpoint_lib gmock)

include(GoogleTest)
gtest_discover_tests(flashpoint_test)