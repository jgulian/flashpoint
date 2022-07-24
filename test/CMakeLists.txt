
target_include_directories(flashpoint_test PUBLIC include)
target_sources(
        flashpoint_test
        PRIVATE

        main.cpp

        raft/in_memory.cpp
        raft/leader_election.cpp
        raft/raft_tester.cpp
)