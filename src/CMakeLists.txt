target_include_directories(flashpoint_lib PUBLIC include)
target_sources(
        flashpoint_lib
        PRIVATE

        cmd/cmd.cpp
        cmd/server.cpp

        containers/channel.cpp
        containers/subject.cpp

        keyvalue/keyvalue.cpp
        keyvalue/storage/simple_storage.cpp
        keyvalue/plugins/grpc.cpp

        raft/raft.cpp
        raft/state.cpp
        raft/util.cpp

        raft/grpc.cpp

        util/containers.cpp
        util/thread_pool.cpp
        util/random.cpp
        util/logger.cpp
)
target_sources(
        flashpoint
        PRIVATE
        main.cpp
)