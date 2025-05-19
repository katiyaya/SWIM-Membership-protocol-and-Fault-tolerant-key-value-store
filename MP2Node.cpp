/**********************************
 * FILE NAME: MP2Node.cpp
 *
 * DESCRIPTION: MP2Node class definition
 **********************************/
#include "MP2Node.h"

/**
 * constructor
 */
MP2Node::MP2Node(Member *memberNode, Params *par, EmulNet * emulNet, Log * log, Address * address) {
	this->memberNode = memberNode;
	this->par = par;
	this->emulNet = emulNet;
	this->log = log;
	ht = new HashTable();
	this->memberNode->addr = *address;
}

/**
 * Destructor
 */
MP2Node::~MP2Node() {
	delete ht;
	delete memberNode;
}
bool MP2Node::areNodeVectorsEqual(vector<Node>& vec1, vector<Node>& vec2) {
     if (vec1.size() != vec2.size()) {
        //cout << "Vectors have different sizes: vec1 size = " << vec1.size() << ", vec2 size = " << vec2.size() << endl;
        return false;
    }
    for (size_t i = 0; i < vec1.size(); ++i) {
        if (vec1[i].getHashCode() != vec2[i].getHashCode()||memcmp(vec1[i].getAddress(), vec2[i].getAddress(), sizeof(char) * 6) != 0) {
            return false;
        }
    }
    return true;
}


/**
 * FUNCTION NAME: updateRing
 *
 * DESCRIPTION: This function does the following:
 * 				1) Gets the current membership list from the Membership Protocol (MP1Node)
 * 				   The membership list is returned as a vector of Nodes. See Node class in Node.h
 * 				2) Constructs the ring based on the membership list
 * 				3) Calls the Stabilization Protocol
 */
void MP2Node::updateRing() {
	vector<Node> curMemList;
	bool change = true;
	curMemList = getMembershipList();
	// Sort the list based on the hashCode
	std::sort(curMemList.begin(), curMemList.end());
	cout<<"Time: "<<par->getcurrtime()<<endl;
	// Debug: Log previous ring
    cout << "Previous ring: ";
    for (auto& node : ring) {
        cout << node.getHashCode() << " ";
    }
    cout << endl;
	//check if previous memberList is equal to the current one
	if (areNodeVectorsEqual(curMemList, ring))
	{
		change = false;		
	}
	//create the ring 
	ring = curMemList;
	 // Debug: Log new ring
    cout << "     New ring: ";
    for (auto& node : ring) {
        cout << node.getHashCode() << " ";
    }
    cout << endl;
	// Run stabilization protocol if the hash table size is greater than zero and if there has been a changed in the ring
	if (change && !ht->isEmpty())
	{
		stabilizationProtocol();
	}	 
	
}

/**
 * FUNCTION NAME: getMemberhipList
 *
 * DESCRIPTION: This function goes through the membership list from the Membership protocol/MP1 and
 * 				i) generates the hash code for each member
 * 				ii) populates the ring member in MP2Node class
 * 				It returns a vector of Nodes. Each element in the vector contain the following fields:
 * 				a) Address of the node
 * 				b) Hash code obtained by consistent hashing of the Address
 */
vector<Node> MP2Node::getMembershipList() {
	unsigned int i;
	vector<Node> curMemList;
	for ( i = 0 ; i < this->memberNode->memberList.size(); i++ ) {
		Address addressOfThisMember;
		int id = this->memberNode->memberList.at(i).getid();
		short port = this->memberNode->memberList.at(i).getport();
		memcpy(&addressOfThisMember.addr[0], &id, sizeof(int));
		memcpy(&addressOfThisMember.addr[4], &port, sizeof(short));
		curMemList.emplace_back(Node(addressOfThisMember));
	}
	return curMemList;
}

/**
 * FUNCTION NAME: hashFunction
 *
 * DESCRIPTION: This functions hashes the key and returns the position on the ring
 * 				HASH FUNCTION USED FOR CONSISTENT HASHING
 */
size_t MP2Node::hashFunction(string key) {
	std::hash<string> hashFunc;
	size_t ret = hashFunc(key);
	return ret%RING_SIZE;
}
bool MP2Node::compareNodeWithMember(Node& node, Member& member) 
{ 
	Address *nodeAddr = node.getAddress(); 
	Address memberAddr = member.addr; 
	return memcmp(nodeAddr->addr, memberAddr.addr, sizeof(memberAddr.addr)) == 0;
}
/**
 * FUNCTION NAME: findNeighbors
 *
 * DESCRIPTION: This function finds the nodes position in the ring, the next two neighbouring nodes
 * in the ring and the previous two neighbours in the ring
 */
void MP2Node::findNeighbors()
{
	hasMyReplicas.clear(); 
	haveReplicasOf.clear();
	// Find my position in the ring 
	auto myPosition = std::find_if(ring.begin(), ring.end(), [this](Node& node) 
	{ 
		return compareNodeWithMember(node, *memberNode); 
	});
	// Find the next two nodes for hasMyReplicas
	for (int i = 1; i <= 2; ++i) 
	{ 
		auto nextNode = myPosition; 
		std::advance(nextNode, i); 
		if (nextNode == ring.end()) 
		{ 
			nextNode = ring.begin(); 
		} 
		hasMyReplicas.push_back(*nextNode); 
	}
	// Find the previous two nodes for haveReplicasOf 
	for (int i = 1; i <= 2; ++i) 
	{ 
		auto prevNode = myPosition; 
		std::advance(prevNode, -i); 
		if (prevNode < ring.begin()) 
		{ 
			prevNode = ring.end() - (ring.begin() - prevNode); 
		} 
		haveReplicasOf.push_back(*prevNode); 
	}
}
/**
 * FUNCTION NAME: dispatchMessages
 *
 * DESCRIPTION: function for dispatching messages to the destination nodes
 */
void MP2Node::dispatchMessages(Message message)
{
	vector<Node> replicas = findNodes(message.key); 
	// Dispatch the message to the target nodes 
	for (Node node : replicas) 
	{ 
		// Serialize the message to a string 
		string serializedMessage = message.toString(); 
		// Send message to the replica node 		
		emulNet->ENsend(&memberNode->addr, &node.nodeAddress, serializedMessage);
	}

}
/**
 * FUNCTION NAME: generateCRUDId, decipherCRUDId
 *
 * DESCRIPTION: help in encoding and decoding additional information to/from the message 
 * (message type and transaction id)
 */
int MP2Node::generateCRUDId(MessageType msgType) 
{ 
	int currentTransID = ++g_transID; 
	// Encode the message type in the first 3 bits 
	int uniqueId = (msgType << 29) | (currentTransID & 0x1FFFFFFF); 
	// Use the lower 29 bits for the transaction ID 
	return uniqueId; 
}
MessageType MP2Node::decipherCRUDId(int CRUDId, int &transID) 
{ 
	// Extract the message type from the first 3 bits 
	MessageType msgType = static_cast<MessageType>((CRUDId >> 29) & 0x7); 
	// Extract the transaction ID from the remaining bits 
	transID = CRUDId & 0x1FFFFFFF; 
	// Mask the lower 29 bits 
	return msgType; 
}
/**
 * Static map to store the KV pairs associated with transaction IDs.
 */
static map<int, pair<string, string>> keyvalueMap;
/**
 * FUNCTION NAME: clientCreate
 *
 * DESCRIPTION: client side CREATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientCreate(string key, string value) {
	int transID = generateCRUDId(CREATE);
	keyvalueMap[transID] = make_pair(key, value);
	Message createKV(transID, memberNode->addr, CREATE, key, value);
	dispatchMessages(createKV);
}

/**
 * FUNCTION NAME: clientRead
 *
 * DESCRIPTION: client side READ API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientRead(string key){
	int transID=generateCRUDId(READ);
	//value is not used anywhere, added for simplicity
	string dummyValue="dummy";
	keyvalueMap[transID]=make_pair(key,dummyValue);
	Message readKV(transID, memberNode->addr, READ, key);
	dispatchMessages(readKV);
}

/**
 * FUNCTION NAME: clientUpdate
 *
 * DESCRIPTION: client side UPDATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientUpdate(string key, string value){
	int transID=generateCRUDId(UPDATE);
	keyvalueMap[transID]=make_pair(key,value);
	Message updateKV(transID, memberNode->addr, UPDATE, key, value);
	dispatchMessages(updateKV);	
}

/**
 * FUNCTION NAME: clientDelete
 *
 * DESCRIPTION: client side DELETE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientDelete(string key){
	int transID = generateCRUDId(DELETE);
	//value is not used anywhere, added for simplicity
	string dummyValue="dummy";
	keyvalueMap[transID] = make_pair(key,dummyValue);
	Message deleteKV(transID, memberNode->addr, DELETE, key);
	dispatchMessages(deleteKV);	
}

/**
 * FUNCTION NAME: createKeyValue
 *
 * DESCRIPTION: Server side CREATE API
 * 			   	The function does the following:
 * 			   	1) Inserts key value into the local hash table
 * 			   	2) Return true or false based on success or failure
 */
bool MP2Node::createKeyValue(string key, string value, int id) {
	// Check if the key is valid
	// Check if the key already exists in the hash table 
	// Check if KV was inserted correctly in the HashTable
	//For containers like std::map or std::unordered_map, which do not allow duplicate keys, 
	//count(key) will return 0 if the key does not exist and 1 if it does.
	if (key.empty() || value.empty()||(ht->count(key) > 0)||!(ht->create(key,value))) 
	{ 
		log->logCreateFail(&memberNode->addr, false, id, key, value);
		return false; 
	} 
	else
	{
		log->logCreateSuccess(&memberNode->addr, false, id, key, value);
		return true;
	}		
}

/**
 * FUNCTION NAME: readKey
 *
 * DESCRIPTION: Server side READ API
 * 			    This function does the following:
 * 			    1) Read key from local hash table
 * 			    2) Return value
 */
string MP2Node::readKey(string key, int id) {
	// Check if the key is valid
	// Check if the key exists in the hash table 
	// Check if value was read correctly from the HashTable
	string value=ht->read(key);
	if (key.empty()||(ht->count(key)==0)||value.empty()) 
	{ 
		log->logReadFail(&memberNode->addr, false, id, key);
		return ""; 
	} 
	else
	{
		log->logReadSuccess(&memberNode->addr, false, id, key, value);
		return value;
	}		
}

/**
 * FUNCTION NAME: updateKeyValue
 *
 * DESCRIPTION: Server side UPDATE API
 * 				This function does the following:
 * 				1) Update the key to the new value in the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::updateKeyValue(string key, string value, int id) {
	// Check if the key is valid
	// Check if the key exists in the hash table 
	// Check if KV was updated correctly in the HashTable
	if (key.empty() || value.empty()||(ht->count(key) == 0)||!(ht->update(key,value))) 
	{ 
		log->logUpdateFail(&memberNode->addr, false, id, key, value);
		return false; 
	} 
	else
	{
		log->logUpdateSuccess(&memberNode->addr, false, id, key, value);
		return true;
	}
}

/**
 * FUNCTION NAME: deleteKey
 *
 * DESCRIPTION: Server side DELETE API
 * 				This function does the following:
 * 				1) Delete the key from the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::deletekey(string key, int id) {
	// Check if the key is valid 
	// Check if the key doesn't exists in the hash table
	// Check if deletion was a success
	if (key.empty()||(ht->count(key)==0)||!(ht->deleteKey(key))) 
	{  
		log->logDeleteFail(&memberNode->addr, false, id, key);
		return false; 
	} 
	else
	{
		log->logDeleteSuccess(&memberNode->addr,false, id, key);
		return true;
	}
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: This function is the message handler of this node.
 * 				This function does the following:
 * 				1) Pops messages from the queue
 * 				2) Handles the messages according to message types
 */
void MP2Node::checkMessages() {
	char * data;
	int size;
	// Local maps to store transaction results 
	map<int, int> successCountMap; 
	map<int, int> replyCountMap; 
	int quorumValue=2;
	int expectedReplies=quorumValue+1;
	//pop message from queue
	while ( !memberNode->mp2q.empty() ) {
		data = (char *)memberNode->mp2q.front().elt;
		size = memberNode->mp2q.front().size;
		memberNode->mp2q.pop();
		//takes the C-style data string and converts it into a std::string object
		//constructor of string class takes ptr to 1st elem const char* 
		//and to the element just past the last char in the range we want to include. 
		string messageStr(data, data + size);
		Message message(messageStr);
		bool isStabilizationMessage = (message.type == CREATE || message.type == DELETE) && (message.replica == PRIMARY || message.replica == SECONDARY || message.replica==TERTIARY);
		switch (message.type) 
		{ 
			case CREATE: 
			{
				if(isStabilizationMessage)
				{
					createKeyValue(message.key, message.value, message.transID);
					cout<<"STABILIZATION protocol CREATE called"<<endl;
				}
				else
				{
					int transID;
					MessageType msgType=decipherCRUDId(message.transID, transID);
					bool success= createKeyValue(message.key, message.value, transID);
					Message replyMessage(message.transID, memberNode->addr, REPLY, success);
					emulNet->ENsend(&memberNode->addr, &message.fromAddr, replyMessage.toString());
				}
			}
			break; 
			
			case READ: 
			{ 
				int transID;
				MessageType msgType=decipherCRUDId(message.transID, transID);
				string value=readKey(message.key, transID);				
				Message replyMessage(message.transID, memberNode->addr, value);
				emulNet->ENsend(&memberNode->addr, &message.fromAddr,replyMessage.toString());
			} 
			break; 
			
			case UPDATE:
			{
				int transID;
				MessageType msgType=decipherCRUDId(message.transID, transID);
				bool success = updateKeyValue(message.key, message.value, transID);
				Message replyMessage(message.transID, memberNode->addr, REPLY, success);
				emulNet->ENsend(&memberNode->addr, &message.fromAddr,replyMessage.toString());				
			}  
			break; 
			
			case DELETE: 
			{
				if(isStabilizationMessage)
				{
					deletekey(message.key, message.transID);
					cout<<"STABILIZATION protocol DELETE called"<<endl;
				}
				else
				{
					int transID;
					MessageType msgType=decipherCRUDId(message.transID, transID);
					bool success=deletekey(message.key, transID);
					Message replyMessage(message.transID, memberNode->addr, REPLY, success);
					emulNet->ENsend(&memberNode->addr, &message.fromAddr, replyMessage.toString());
				}
			}
			break; 
			case REPLY: 
			{
				int transID;
				string key;
				string value;
				MessageType msgType = decipherCRUDId(message.transID, transID);
				
				auto it = keyvalueMap.find(message.transID); 
				if (it != keyvalueMap.end()) 
				{ 
					key = it->second.first; 
					value =it->second.second;
				}
				else
				{
					//cout<<"keyvalue not found in coordinator's keyvalueMap"<<endl;
				}
				// Initialize maps if transaction ID is new 
				if (replyCountMap.find(message.transID) == replyCountMap.end()) 
				{ 
					successCountMap[message.transID] = 0; 
					replyCountMap[message.transID] = 0; 
				}
				// Increment the counts 
				replyCountMap[message.transID]++; 
				if (message.success) 
				{ 
					successCountMap[message.transID]++; 
				}
				//if the keyvalue pair exists in the coordinator's map
				if(it!=keyvalueMap.end())
				{
					bool logged=false;			
				// Check if the quorum is met 
				if (successCountMap[message.transID] >= quorumValue) 
				{ 
					//cout<<"Qourum reached with successful replies, logging"<<endl;
					// Log the result based on quorum 
					if (msgType == CREATE) 
					{ 
						log->logCreateSuccess(&memberNode->addr, true, transID, key, value); 
					} 
					else if (msgType == DELETE) 
					{ 
						log->logDeleteSuccess(&memberNode->addr, true, transID, key); 
					} 
					else if(msgType==READ)
					{
						log->logReadSuccess(&memberNode->addr, true, transID, key, value);
					}
					else if (msgType == UPDATE) 
					{ 
						log->logUpdateSuccess(&memberNode->addr, true, transID, key, value); 
					} 
					logged=true;
																
				}
				else if (replyCountMap[message.transID]==expectedReplies) 
				{
					cout << "All replies received, logging failure if quorum not met" << endl;
					if(successCountMap[transID] < quorumValue)
					{ 
					if (msgType == CREATE) 
					{ 
						log->logCreateFail(&memberNode->addr, true, transID, key, value); 
					} 
					else if (msgType == DELETE) 
					{ 
						log->logDeleteFail(&memberNode->addr, true, transID, key); 
					}
					else if(msgType==READ)
					{
						log->logReadFail(&memberNode->addr, true, transID, key);
					} 
					else if (msgType == UPDATE) 
					{ 
						log->logUpdateFail(&memberNode->addr, true, transID, key, value); 
					} 
					}
					logged=true;
				}
				if (logged)
				{
					// Remove the KV pair from the maps after logging 
					keyvalueMap.erase(it);
					successCountMap.erase(transID); 
					replyCountMap.erase(transID);
				}				
				}		
							
			}			
			break; 
			default: // process READREPLY here
			{
				int transID;
				string key;
				MessageType msgType = decipherCRUDId(message.transID, transID);
				auto it = keyvalueMap.find(message.transID); 
				if (it != keyvalueMap.end()) 
				{ 
					key = it->second.first; 
				}
				else
				{
					cout<<"key not found in coordinator's keyvalueMap"<<endl;
				}
				bool success = message.value.empty() ? false : true;
				// Initialize maps if transaction ID is new 
				if (replyCountMap.find(message.transID) == replyCountMap.end()) 
				{ 
					successCountMap[message.transID] = 0; 
					replyCountMap[message.transID] = 0; 
				}
				// Increment the counts 
				replyCountMap[message.transID]++; 
				if (success) 
				{ 
					successCountMap[message.transID]++; 
				}
				//if the keyvalue pair exists in the coordinator's map
				if(it!=keyvalueMap.end())
				{
					bool logged=false;			
				// Check if the quorum is met 
				if (successCountMap[message.transID] >= quorumValue) 
				{ 
					// Log the result based on quorum 
					log->logReadSuccess(&memberNode->addr, true, transID, key, message.value);
					logged=true;
																
				}
				else if (replyCountMap[message.transID]==expectedReplies) 
				{
					cout << "All replies received, logging failure if quorum not met" << endl;
					if(successCountMap[transID] < quorumValue)
					log->logReadFail(&memberNode->addr, true, transID, key);
					logged=true;
				}
				if (logged)
				{
					// Remove the KV pair from the maps after logging 
					keyvalueMap.erase(it);
					successCountMap.erase(transID); 
					replyCountMap.erase(transID);
				}				
				}		

			} 
			break; 
		}
	}
	//after processing all messages log failure for remaining replies
    for (const auto& reply : replyCountMap) {
        int transID = reply.first;
        int replies = reply.second;
		int dechipheredTransID;        
        auto it = keyvalueMap.find(transID);
        if (it != keyvalueMap.end()) {
        string key = it->second.first;
        string value = it->second.second;
        MessageType msgType = decipherCRUDId(transID, dechipheredTransID);
        if (msgType == CREATE) {
        log->logCreateFail(&memberNode->addr, true, dechipheredTransID, key, value);
        } else if (msgType == DELETE) {
        log->logDeleteFail(&memberNode->addr, true, dechipheredTransID, key);
        } else if (msgType == READ) {
        log->logReadFail(&memberNode->addr, true, dechipheredTransID, key);
        } else if (msgType == UPDATE) {
        log->logUpdateFail(&memberNode->addr, true, dechipheredTransID, key, value);
        }
		else {cout<<"Unknown message type"<<endl;}        
    }
	}
	

}

/**
 * FUNCTION NAME: findNodes
 *
 * DESCRIPTION: Find the replicas of the given keyfunction
 * 				This function is responsible for finding the replicas of a key
 */
vector<Node> MP2Node::findNodes(string key) {
	size_t pos = hashFunction(key);
	vector<Node> addr_vec;
	if (ring.size() >= 3) {
		// if pos <= min || pos > max, the leader is the min
		if (pos <= ring.at(0).getHashCode() || pos > ring.at(ring.size()-1).getHashCode()) {
			addr_vec.emplace_back(ring.at(0));
			addr_vec.emplace_back(ring.at(1));
			addr_vec.emplace_back(ring.at(2));
		}
		else {
			// go through the ring until pos <= node
			for (int i=1; i<ring.size(); i++){
				Node addr = ring.at(i);
				if (pos <= addr.getHashCode()) {
					addr_vec.emplace_back(addr);
					addr_vec.emplace_back(ring.at((i+1)%ring.size()));
					addr_vec.emplace_back(ring.at((i+2)%ring.size()));
					break;
				}
			}
		}
	}
	return addr_vec;
}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: Receive messages from EmulNet and push into the queue (mp2q)
 */
bool MP2Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), this->enqueueWrapper, NULL, 1, &(memberNode->mp2q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue of MP2Node
 */
int MP2Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

//functions for the stabilization protocol
int MP2Node::generateTransactionID() 
{ 
	return g_transID++; 
}
void MP2Node::sendReplicateMessage(Node newReplicaNode, string key, string value, ReplicaType replicaType) 
{
	int transID = generateTransactionID();
	Message replicateMessage(transID, memberNode->addr, CREATE, key, value, replicaType);
	// Serialize the message to a string 
	string serializedMessage = replicateMessage.toString();
	emulNet->ENsend(&memberNode->addr, newReplicaNode.getAddress(), serializedMessage);
	cout<<"SendReplicateMessage called from stabilization protocol"<<endl; 	
}
void MP2Node::sendDeleteMessage(Node excessNode, string key)
{
	int transID = generateTransactionID();
	Message replicateMessage(transID, memberNode->addr, DELETE, key);
	// Serialize the message to a string 
	string serializedMessage = replicateMessage.toString();
	emulNet->ENsend(&memberNode->addr, excessNode.getAddress(), serializedMessage);
	cout<<"SendDeleteMessage called from stabilization protocol"<<endl;
}

//helper function
Node MP2Node::findNextNode(Node &currentNode) 
{ 
	auto it = find_if(ring.begin(), ring.end(), 
	[&currentNode](const Node& n) 
	{ 
		return NodeComparator()(n, currentNode); 
	});
	++it; 
	if (it == ring.end()) 
	{ 
		it = ring.begin();
	} 
	return *it; 
}
/**
 * FUNCTION NAME: stabilizationProtocol
 *
 * DESCRIPTION: This runs the stabilization protocol in case of Node joins and leaves
 * 				It ensures that there always 3 copies of all keys in the DHT at all times
 * 				The function does the following:
 *				1) Ensures that there are three "CORRECT" replicas of all the keys in spite of failures and joins
 *				Note:- "CORRECT" replicas implies that every key is replicated in its two neighboring nodes in the ring
 */
void MP2Node::stabilizationProtocol() 
{
	// Store old replicas information
	vector<Node> oldHasMyReplicas = hasMyReplicas;
    vector<Node> oldHaveReplicasOf = haveReplicasOf;
	//find new neighbors based on the new ring structure for hasMyReplicas and haveReplicasOf
	findNeighbors();
	//Iterate through each key in the hash table 
	for (const auto &entry : ht->hashTable) 
	{ 
		string key = entry.first; 
		string value = entry.second;
		//find the nodes that contain this key including the original node and the next two nodes
		vector<Node> replicas = findNodes(key);
		cout << "Old replicas: ";
        for (auto& node : oldHasMyReplicas) {
            cout << node.getHashCode() << " ";
        }
		 cout << "\nNew replicas: ";
        for (auto& node : replicas) {
            cout << node.getHashCode() << " ";
        }
        cout << endl;
		// Check if current node should hold this key
        Node currentNode = Node(memberNode->addr);
        if (std::find_if(replicas.begin(), replicas.end(), [&currentNode](Node& node) {
                return memcmp(node.getAddress(), currentNode.getAddress(), sizeof(char) * 6) == 0;
            }) == replicas.end()) {
            // Current node should not hold this key, send a delete message
            sendDeleteMessage(currentNode, key);
            continue;
        }
		//Find replicas that need to be added
        for (Node& newNode : hasMyReplicas) {
			//This comparison checks whether the newNode was found in the oldHasMyReplicas vector.
            if (std::find_if(oldHasMyReplicas.begin(), oldHasMyReplicas.end(), 
			[&newNode](Node& oldNode) { 
                        return memcmp(newNode.getAddress(), oldNode.getAddress(), sizeof(char) * 6) == 0; 
                    }) == oldHasMyReplicas.end()) {
				//if statement is executed when a match is not found for newNode in the oldHasMyReplicas vector
				// Determine the replica type based on the replicas array
                auto it = std::find_if(replicas.begin(), replicas.end(), [&newNode](Node& node) {
                return memcmp(node.getAddress(), newNode.getAddress(), sizeof(char) * 6) == 0;
                });
                int pos = std::distance(replicas.begin(), it);
				ReplicaType replicaType = (pos == 0) ? PRIMARY : (pos == 1) ? SECONDARY : TERTIARY;
               	sendReplicateMessage(newNode, key, value, replicaType);
            }
        }
		    
	}
}
