# :zap: Flashpoint

Pre-release alpha goals

1. Get core service working
   1. Create file config for raft
   2. Make key value start configs and commands
2. Make keyvalue callback based
3. Clean up code
   1. Make raft log entry data use configs without serialization

Official Release goals

* increase warnings and generally clean up code
* write tests
* add clang support
* start config on leader cmd
* allow users to create (yaml) config file for plugins and server
* use ASan in tests
* use concurrent channel (like linked list) for apply msg
* rethink threading