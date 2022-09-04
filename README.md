# :zap: Flashpoint Key-Value

Pre-release alpha goals (Target: Sep 4)

1. Get core service working
   1. Get general 3 participant replication work

Alpha goals (Target: Oct 2)

1. Make keyvalue callback based
2. Clean up code
   1. Make raft log entry data use configs without serialization
   2. Enhance naming (raft config -> raft settings)
   3. Review math (simplify prev log index)
   4. Make config me a reference to peers_ me
   5. Decide on a code style
   6. Remove Start from grpc
3. Support snapshots
4. Support sharding
5. Review TODOs

Official Release goals (Target: Dec 22)

* increase warnings and generally clean up code
* write tests
* add clang support
* start config on leader cmd
* allow users to create (yaml) config file for plugins and server
* use ASan in tests
* Aries