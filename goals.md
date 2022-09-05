Alpha goals (Target: Oct 2)

1. Make keyvalue callback based
2. Clean up code
    1. Make raft log entry data use configs without serialization
    2. Enhance naming (raft config -> raft settings)
    3. Review math (simplify prev log index)
    4. Make config me a reference to peers_ me
    5. Decide on a code style
3. Tune election timing parameters
4. Support persistence & snapshots
5. Support sharding
6. Review TODOs

Official Release goals (Target: Dec 22)

* increase warnings and generally clean up code
    *
* write tests
* add clang support
* start config on leader cmd
* allow users to create (yaml) config file for plugins and server
* use ASan in tests
* Aries