/**********************************
 * FILE NAME: MP2Node.h
 *
 * DESCRIPTION: MP2Node class header file
 **********************************/

#ifndef MP2NODE_H_
#define MP2NODE_H_

/**
 * Header files
 */
#include "stdincludes.h"
#include "EmulNet.h"
#include "Node.h"
#include "HashTable.h"
#include "Log.h"
#include "Params.h"
#include "Message.h"
#include "Queue.h"

/**
 * CLASS NAME: MP2Node
 *
 * DESCRIPTION: This class encapsulates all the key-value store functionality
 * 				including:
 * 				1) Ring
 * 				2) Stabilization Protocol
 * 				3) Server side CRUD APIs
 * 				4) Client side CRUD APIs
 */
class MP2Node {
private:
	// Vector holding the next two neighbors in the ring who have my replicas
	vector<Node> hasMyReplicas;
	// Vector holding the previous two neighbors in the ring whose replicas I have
	vector<Node> haveReplicasOf;
	// Ring
	vector<Node> ring;
	// Hash Table
	HashTable * ht;
	// Member representing this member
	Member *memberNode;
	// Params object
	Params *par;
	// Object of EmulNet
	EmulNet * emulNet;
	// Object of Log
	Log * log;

public:
	MP2Node(Member *memberNode, Params *par, EmulNet *emulNet, Log *log, Address *addressOfMember);
	Member * getMemberNode() {
		return this->memberNode;
	}
	// ring functionalities
	bool areNodeVectorsEqual(vector<Node>&, vector<Node>&);
	void updateRing();
	vector<Node> getMembershipList();
	size_t hashFunction(string key);
	bool compareNodeWithMember(Node& node, Member& member);
	void findNeighbors();
	Node findNextNode(Node &currentNode);
	struct NodeComparator { 
		bool operator()(const Node& n1, const Node& n2) const 
		{ 
			return memcmp(n1.nodeAddress.addr, n2.nodeAddress.addr, sizeof(char) * 6)==0;
		} 
	};

	// client side CRUD APIs
	int generateCRUDId(MessageType msgType);
	MessageType decipherCRUDId(int uniqueId, int &transID);
	void clientCreate(string key, string value);
	void clientRead(string key);
	void clientUpdate(string key, string value);
	void clientDelete(string key);

	// receive messages from Emulnet
	bool recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);

	// handle messages from receiving queue
	void checkMessages();

	// coordinator dispatches messages to corresponding nodes
	void dispatchMessages(Message message);

	// find the addresses of nodes that are responsible for a key
	vector<Node> findNodes(string key);

	// server
	bool createKeyValue(string key, string value, int id);
	string readKey(string key, int id);
	bool updateKeyValue(string key, string value, int id);
	bool deletekey(string key, int id);
	int generateTransactionID();
	void sendReplicateMessage(Node newReplicaNode, string key, string value, ReplicaType replicaType);
	void sendDeleteMessage(Node excessNode, string key);
	// stabilization protocol - handle multiple failures
	void stabilizationProtocol();

	~MP2Node();
};

#endif /* MP2NODE_H_ */
