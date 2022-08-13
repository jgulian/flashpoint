# Flashpoint

Todo list

1. build key value raft plugin

Pre-release goals

1. Get core service working
2. Clean up code

Long term todo list

* increase warnings and generally clean up code
* write tests
* add clang support
* start config on leader cmd
* allow users to create (yaml) config file for plugins and server
* use ASan in tests
* rethink threading
    * It may be good to remove the thread pool and just use grpc
      * would need to update how updating followers works because it could get messy, although this is definitely possible
    * if it is not the thread pool should be used as much as possible and the following should be adopted
        * allow users to specify how many threads for the thread pool.
        * don't make raft create a thread use a time based scheduler like the next point.
        * do simple scheduling in thread pool and make logger schedule every second or so