Goals for the week (Sep 5)

* Update variable names to agree with `.clang-tidy`
* Maybe callback KV

Alpha goals (Target: Oct 2)

1. Make keyvalue callback based
2. Clean up code
   1. Make raft log entry data use configs without serialization
   2. Enhance naming (raft config -> raft settings)
   3. Review math (simplify prev log index)
   4. Make config me a reference to peers_ me
   5. ~~Decide on a code style~~
      1. Update variable names
   6. make leaders/followers defensive (throw exception if leader_id() is wrong in AE/IS)
3. Tune election timing parameters
4. ~~Support Log Compaction: persistence & snapshots~~
   1. Load from persistent state
   2. Make sure temp snapshot is temp
   3. Add locking to snapshot
5. Support sharding
6. Review TODOs
7. Use environment variables for things like logging color and log level

Official Release goals (Target: Dec 22)

* increase defense programming and generally clean up code
* write tests
* move to clang
* start config on leader cmd
* allow users to create (yaml) config file for plugins and server
* use ASan in tests
* Aries
* Log to file by default