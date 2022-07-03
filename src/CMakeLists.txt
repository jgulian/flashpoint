target_include_directories(flashpoint_lib PUBLIC include)
target_sources(
        flashpoint_lib
        PRIVATE

        containers/channel.cpp

        public/grpc.cpp

        raft/clerk.cpp
        raft/peer.cpp
        raft/protocol.cpp
        raft/raft.cpp
        raft/state.cpp
        raft/util.cpp

        raft/protocols/grpc.cpp

        storage/document.cpp

        storage/engines/json_engine.cpp
        storage/engines/logger_engine.cpp
        storage/engines/proto_engine.cpp
        storage/engines/raft_clerk_engine.cpp

        util/thread_pool.cpp
)
target_sources(
        flashpoint
        PRIVATE
        main.cpp
)