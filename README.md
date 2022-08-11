# Flashpoint

Todo list

1. build key value raft plugin

Pre-release goals

1. Get core service working
2. Clean up code

Long term todo list

* increase warnings and generally clean up code
* don't make raft create a thread.
* do simple scheduling in thread pool and make logger schedule every second or so
* allow users to specify how many threads for the thread pool.
* write tests
* add clang support
* start config on leader cmd
* allow users to disable join cluster rpc
* allow users to create (yaml) config file for plugins and server