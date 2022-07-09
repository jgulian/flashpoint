target_include_directories(flashpoint_test PUBLIC include)
target_sources(
        flashpoint_test
        PRIVATE

        main.cpp

        raft/cluster.cpp
        raft/in_memory.cpp
)