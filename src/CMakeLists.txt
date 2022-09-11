target_include_directories(flashpoint_lib PUBLIC include)
target_sources(
        flashpoint_lib
        PRIVATE

        cmd/cmd.cpp

        keyvalue/keyvalue.cpp

        raft/client.cpp
        raft/errors.cpp
        raft/raft.cpp

        util/env.cpp
        util/random.cpp
        util/logger.cpp
)
target_sources(
        flashpoint
        PRIVATE
        main.cpp
)