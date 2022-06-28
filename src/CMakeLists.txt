target_include_directories(flashpoint_lib PUBLIC include)
target_sources(
        flashpoint_lib
        PRIVATE

        public/grpc.cpp

        raft/raft.cpp

        storage/document.cpp

        storage/engines/json_engine.cpp
        storage/engines/logger_engine.cpp
        storage/engines/proto_engine.cpp
        storage/engines/raft_clerk_engine.cpp
)
target_sources(
        flashpoint
        PRIVATE
        main.cpp
)