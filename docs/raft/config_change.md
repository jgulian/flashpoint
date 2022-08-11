# Raft Config Change Documentation

Updating a config is the same as prescribed by the raft algorithm. However, this also supports a possible client
requesting to join the cluster. This is described below.

## Join Cluster Protocol

* Client initiates server
* Client makes `JoinCluster` RPC to cluster member
    * If cluster member is not leader, return info about the leader and force client to retry.
    * If cluster member is leader, continue
* Cluster member attempts to make `AppendEntries` (`term=0`) call to client
    * If call is unsuccessful, reply unsuccessful to the original `JoinCluster` RPC call.
    * If the call is successful, return the config to the client.
* Wait until config is committed.
    * If new config is erased, make `AppendEntries` (`term=-1`) call to client. The client must start over.
    * Once the config is committed, the cluster member initiates an `AppendEntries` (`term=1`) call to client.
      the client stops fake server, and starts real server.
* Run necessary install snapshots, and append entries until the client is up-to-date.
* Start config with client as voting member.
* Client can now vote.