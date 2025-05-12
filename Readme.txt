About the projects: 
the implemented code is in the peer-to-peer layer in files MP1Node.{cpp,h} and MP2Node.{cpp,h}
MP1Node.{cpp,h} inmplements SWIM-style membership protocol along with some elements of gossip-style heartbeating communication, which ensures robust message exchanging system, that ensures that each node receives up-to-date information about the status of its neighbours in the cluster and thus maintain the "view" (alive nodes) in the cluster
Unit tests (autograder script) is provided to test that the programm passes all requerements.
1. Membership protocol MP1Node.{cpp,h} was tested successfully (90 out of 90 points )in 3 scenarios and
graded each of them on 3 separate metrics. The scenarios are as follows:
1. Single node failure
2. Multiple node failure
3. Single node failure under a lossy network.
The grader tested the following things: 1) whether all nodes joined the peer group correctly, 2) whether all
nodes detected the failed node (completeness) and 3) whether the correct failed node was detected
(accuracy).
