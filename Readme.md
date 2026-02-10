## Membership protocol MP1Node.{cpp,h}
The first project (MP1Node.{cpp,h})  is about implementing a membership protocol. A membership protocol is a mechanism used in distributed systems to maintain a list of active nodes in a network. It helps nodes discover peers, track failures, and manage group membership dynamically. My version of the protocol implements SWIM-style membership protocol elements along with some elements of gossip-style heartbeating communication, which ensure robust message exchanging system, that ensures that each node receives up-to-date information about the status of its neighbours in the cluster and thus maintain the "view" (information about node leaves, joins, failures, ect.) in the cluster.
Since it is infeasible to run a thousand cluster nodes (peers) over a real network, an implementation of an emulated network layer (EmulNet) was provided. The membership
protocol implementation sits above EmulNet in a peer- to-peer (P2P) layer, but below an App layer.
The implemented code is in files MP1Node.{cpp,h}. Unit tests (autograder script) are provided to test that the programm passes all requerements.
     
**Membership protocol MP1Node.{cpp,h} was tested successfully (90 out of 90 points received) in 3 scenarios and
graded each of them on 3 separate metrics.** The scenarios are as follows:
1. Single node failure
2. Multiple node failure
3. Single node failure under a lossy network.
   
**The grader tested the following things:**
1) whether all nodes joined the peer group correctly,
2) whether all nodes detected the failed node (completeness) and
3) whether the correct failed node was detected (accuracy).

## Fault-tolerant key-value store MP2Node.{cpp,h}
The second project (MP2Node.{cpp,h}) is about building a fault-tolerant key-value store on top of a provided emulated network layer (EmulNet) and the implemented previously membership protocol, under the App layer. The membership list is derived from the membership protocol layer and used to maintain the "view" of the virtual ring (cluster). 

The key-value store talks to the membership protocol and receives from it the membership list. Then it uses it to maintain its view of the virtual ring. Periodically, each node engages in the membership protocol to try to bring its membership list up to date.
![Screenshot (39)1](https://github.com/user-attachments/assets/c738442a-abe8-4d43-a468-5d8b9634351d)

**The functionalities implemented in the key-value store (among the active members of the ring received through membership protocol list) are:** 
1. CRUD operations (Create, Read, Update, Delete).
2. Load-balancing (via a consistent hashing ring to hash both servers and keys).
3. Fault-tolerance up to two failures (by replicating each key three times to three successive nodes
in the ring, starting from the first node at or to the clockwise of the hashed key).
4. Quorum consistency level for both reads and writes (at least two replicas).
5. Stabilization after failure (recreated three replicas after failure).

**The Key-value store MP2Node.{cpp,h} was tested successfully (90 out of 90 points received) in the following testcases:**
1. Basic CRUD tests that test if three replicas respond
2. Single failure followed immediately by operations which should succeed (as quorum can still be
reached with 1 failure)
3. Multiple failures followed immediately by operations which should fail as quorum cannot be
reached
4. Failures followed by a time for the system to re-stabilize, followed by operations that should
succeed because the key has been re-replicated again at three nodes.

**To test:**
View the Grader Script Output
1. Click on "Actions" tab -> "Actions Workflow" tab -> "update build.yml" (with the green check ✅)
2. Inside the workflow run, look for the "Run Key-value store grader for testing", expand it to see the output of ./KVStoreGrader.sh
   
The grader is provided only for MP2Node.{cpp,h} in this repository, but it passing the tests implies that a working membership protocol is already implemented (MP1Node.{cpp,h}), so there is no need to provide a separate test for MP1Node.{cpp,h}.
     
**! Note !** 
The code I developed is contained in:
1. MP2Node.{cpp,h} (Key-value store logic)
2. MP1Node.{cpp,h} (Membership protocol)

This implementation relies on C-style memory management (memcpy, malloc, raw pointers, etc.), ensuring a distinct coding style throughout my work. This approach also shows that the algorithm design was developed from scratch, without incorporating external code.
