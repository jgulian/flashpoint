target_include_directories(flashpoint_lib PUBLIC include)
target_sources(
        flashpoint_lib
        PRIVATE

        cmd/cmd.cpp
        cmd/server.cpp

        keyvalue/keyvalue.cpp
        keyvalue/plugins/raft.cpp
        keyvalue/storage/simple_storage.cpp

        raft/errors.cpp
        raft/raft.cpp

        util/encoding.cpp
        util/random.cpp
        util/logger.cpp
)
target_sources(
        flashpoint
        PRIVATE
        main.cpp
)