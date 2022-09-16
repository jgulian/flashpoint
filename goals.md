Goals for the week (Sep 5)

* Update variable names to agree with `.clang-tidy`
* Maybe callback KV

Alpha goals (Target: Oct 2)

1. Make keyvalue callback based
2. Clean up code
   1. Make raft Log entry data use configs without serialization
   2. Enhance naming (raft config -> raft settings)
   3. Review math (simplify prev Log index)
   4. Make config me a reference to peers_ me
   6. make leaders/followers defensive (throw exception if leader_id() is wrong in AE/IS)
   7. rethink what needs to be a pointer
   8. Move `KeyValueService::UpdateSnapshot` file io to Raft
3. Tune election timing parameters
4. ~~Support Log Compaction: persistence & snapshots~~
   1. Load from persistent state
   2. Make sure temp Snapshot is temp
   3. Add locking to Snapshot
5. Support sharding
6. Review TODOs
7. Use environment variables for things like logging color and Log level

Official Release goals (Target: Dec 22)

* increase defense programming and generally clean up code
* write tests
* move to clang
* Start config on leader cmd
* allow users to create (yaml) config file for plugins and server
* use ASan in tests
* Aries
* Log to file by default
* Allow secure grpc connections