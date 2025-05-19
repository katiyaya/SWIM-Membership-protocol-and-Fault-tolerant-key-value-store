/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}
void MP1Node::printMemberList(const char*location) 
{ 
   std::cout<<location<<endl;
   std::cout << "Contents of memberList:" << std::endl;
 // Iterate through the memberList and print each entry's details 
 for (const auto& entry : memberNode->memberList) 
 { std::cout << "ID: " << entry.id << ", Port: " << entry.port << ", Heartbeat: " << entry.heartbeat << ", Timestamp: " << entry.timestamp << std::endl; }
  // Print the position of myPos if it's valid 
  if (memberNode->myPos != memberNode->memberList.end()) 
  { std::cout << "myPos points to: ID: " << memberNode->myPos->getid() << ", Port: " << memberNode->myPos->getport() << ", Heartbeat: " << memberNode->myPos->getheartbeat() << ", Timestamp: " << memberNode->myPos->gettimestamp() << std::endl; } 
  else { std::cout << "myPos is invalid." << std::endl; } 
  
}
/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);
    
	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode,id,port);
    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long)+sizeof(long)+ 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        long currtime=par->getcurrtime();
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr)+sizeof(long),&currtime,sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
    memberNode->memberList.clear();
    memberNode->heartbeat = 0; 
    memberNode->pingCounter = TFAIL;
    // Cleanup the network 
    emulNet->ENcleanup(); 
    return 0; 
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}
/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}
/**
 * FUNCTION NAME: updateMemberList
 *
 * DESCRIPTION: Update the receivers membership list and send gossip message upon addition of a new member
 */
void MP1Node::updateMemberList(MessageHdr*msg)
{
    Address SenderAddress;
    int id;
	short port;
    long heartbeat;
    long timestamp;    
    memcpy(&SenderAddress.addr, (char *)(msg + 1), sizeof(SenderAddress.addr));
    memcpy(&heartbeat,(char *)(msg+1) + 1 + sizeof(SenderAddress.addr), sizeof(long));
    memcpy(&timestamp,(char*)(msg+1)+ 1 + sizeof(SenderAddress.addr)+sizeof(long), sizeof(long));
    memcpy(&id, &SenderAddress.addr[0], sizeof(int));
	memcpy(&port, &SenderAddress.addr[4], sizeof(short));
    size_t myPosIndex = std::distance(memberNode->memberList.begin(), memberNode->myPos);
    bool exists=false;
    for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
    {
        if (it->id==id&&it->port==port)
        {
            exists=true;
            if(heartbeat>it->heartbeat)
            {
                it->setheartbeat(heartbeat);
                it->settimestamp(par->getcurrtime());
            }
            break;
        }
    }
    if(!exists)
    {
        memberNode->memberList.push_back(MemberListEntry(id,port,heartbeat,par->getcurrtime()));
        #ifdef DEBUGLOG
        log->logNodeAdd(&memberNode->addr, &SenderAddress);
        #endif  
        memberNode->myPos = memberNode->memberList.begin() + myPosIndex;           
        //send GOSSIP with TTL=3 to 2 random nodes
        int ttl=TTL;
        bool AddOrUpdate=true;
        size_t msgSize=sizeof(MessageHdr)+sizeof(int)+sizeof(MemberListEntry)+sizeof(bool);
        MessageHdr* gossip =(MessageHdr*)malloc(msgSize*sizeof(char));
        gossip->msgType=GOSSIP;
        MemberListEntry entry{id,port,heartbeat,par->getcurrtime()};
        memcpy((char*)(gossip+1), &ttl, sizeof(int));
        memcpy((char*)(gossip+1)+sizeof(int), &entry, sizeof(MemberListEntry));
        memcpy((char*)(gossip+1)+sizeof(int)+sizeof(MemberListEntry),&AddOrUpdate,sizeof(bool));
        sendGossip(gossip,msgSize);
        free(gossip);
    }
    return;
                 
}
//overload of the function above
void MP1Node::updateMemberList(MessageHdr*msg, int size)
{   
    Address SenderAddress;
    memcpy(&SenderAddress.addr, (char *)(msg + 1), sizeof(SenderAddress.addr));
    size_t myPosIndex = std::distance(memberNode->memberList.begin(), memberNode->myPos);
    char* ptr = (char*)(msg + 1);
    ptr+=sizeof(memberNode->addr.addr)*2 + sizeof(bool);
    while(ptr-(char*)msg<size)
    {
        MemberListEntry sendersEntry;
        memcpy(&sendersEntry, ptr, sizeof(MemberListEntry));
        ptr+=sizeof(MemberListEntry);
        bool addOrupdate;
        memcpy(&addOrupdate,ptr,sizeof(bool));
        //move to the next memberListEntry
        ptr += sizeof(bool);
        if(addOrupdate)
        {
            bool exists = false;
            for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
            {
                if(it->id==sendersEntry.id&&it->port==sendersEntry.port)
                {
                    exists=true;
                    if(sendersEntry.heartbeat>it->heartbeat)
                    {
                        it->setheartbeat(sendersEntry.heartbeat);
                        it->settimestamp(par->getcurrtime());
                    }
                    break;
                }
            }
            if(!exists)
            {
                memberNode->memberList.push_back(MemberListEntry(sendersEntry.id,sendersEntry.port,sendersEntry.heartbeat,par->getcurrtime()));
                #ifdef DEBUGLOG
                log->logNodeAdd(&memberNode->addr, &SenderAddress);
                #endif
                memberNode->myPos = memberNode->memberList.begin() + myPosIndex;
                //send GOSSIP with TTL=3 to 2 random nodes
                int ttl=TTL;
                bool AddOrUpdate=true;
                size_t msgSize=sizeof(MessageHdr)+sizeof(int)+sizeof(MemberListEntry)+sizeof(bool);
                MessageHdr* gossip =(MessageHdr*)malloc(msgSize*sizeof(char));
                gossip->msgType=GOSSIP;
                MemberListEntry entry{sendersEntry.id,sendersEntry.port,sendersEntry.heartbeat,par->getcurrtime()};
                memcpy((char*)(gossip+1), &ttl, sizeof(int));
                memcpy((char*)(gossip+1)+sizeof(int), &entry, sizeof(MemberListEntry));
                memcpy((char*)(gossip+1)+sizeof(int)+sizeof(MemberListEntry),&AddOrUpdate,sizeof(bool));
                sendGossip(gossip,msgSize);
                free(gossip);               
            }
            
        }
    }
}
/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	 MessageHdr *msg = (MessageHdr *)data;
     switch (msg->msgType) {
        /*node receives join request message, updates its membership list and sends a reply
        that contains the nodes own membership list*/
        case JOINREQ:{
            Address SenderAddress;
            memcpy(&SenderAddress.addr,(char*)(msg+1), sizeof(SenderAddress.addr));
            updateMemberList(msg);            
            //preparing JOINREP msg
            size_t actualSize = memberNode->memberList.size();
            size_t listSize = std::min(actualSize, static_cast<size_t>(MAX_PARTIAL_LIST_SIZE)) * sizeof(MemberListEntry);
            size_t msgSize = sizeof(MessageHdr) + sizeof(memberNode->addr.addr)+listSize;
            MessageHdr *reply = (MessageHdr *)malloc(msgSize);
            reply->msgType=JOINREP;
            memcpy((char*)(reply+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
            memcpy((char*)(reply+1)+sizeof(SenderAddress.addr), memberNode->memberList.data(), listSize);
            emulNet->ENsend(&memberNode->addr, &SenderAddress, (char *)reply, msgSize);
            free(reply);
            break;
        }
        /*node receives join reply message, extracts the membership list and merges it with its own,
          sends gossip message about newly added entries to the membership list*/
        case JOINREP: {
            Address SenderAddress;
            memcpy(&SenderAddress.addr, (char *)(msg + 1), sizeof(SenderAddress.addr));
            // Extract and process the member list from the JOINREP message
            size_t listOffset = sizeof(SenderAddress.addr);
            MemberListEntry *members = (MemberListEntry *)((char *)(msg + 1) + listOffset);//pointer to memberList.data()
            size_t listSize = (size - sizeof(MessageHdr) - listOffset) / sizeof(MemberListEntry);
            size_t myPosIndex = std::distance(memberNode->memberList.begin(), memberNode->myPos);
            /* Here std::unordered_set may be used when merging two lists to reduce O(n2) complexity to O(n)
            but I decided to maintain the existing style as this codebase uses heavy
            C-style constructs */
            for (size_t i = 0; i < listSize; ++i) 
            {
                bool exists=false;
                for(auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
                {
                    if(members[i].id==it->id&&members[i].port==it->port)
                    {
                        exists=true;
                        if(members[i].heartbeat>it->heartbeat)
                        {
                            it->setheartbeat(members[i].heartbeat);
                            it->settimestamp(par->getcurrtime());
                        }
                        break;
                    }
                }
                if(!exists)
                {
                  Address addr=getAddr(members[i].id,members[i].port);
                  memberNode->memberList.push_back(members[i]);
                  #ifdef DEBUGLOG
                  log->logNodeAdd(&memberNode->addr, &addr);
                  #endif
                }                                                
            }
            memberNode->myPos = memberNode->memberList.begin() + myPosIndex;
            memberNode->inGroup = true;
            // Prepare a GOSSIP with TTL=3 to 2 random nodes 
            size_t actualSize = memberNode->memberList.size();
            size_t listsize = std::min(actualSize, static_cast<size_t>(MAX_PARTIAL_LIST_SIZE)); 
            int ttl = TTL; 
            bool AddOrUpdate = true; 
            size_t msgSize = sizeof(MessageHdr) + sizeof(int) + sizeof(MemberListEntry) * listsize + sizeof(bool) * listsize; 
            MessageHdr* gossip = (MessageHdr*)malloc(msgSize * sizeof(char)); 
            gossip->msgType = GOSSIP; 
            memcpy((char*)(gossip + 1), &ttl, sizeof(int)); 
            char* ptr = (char*)(gossip + 1) + sizeof(int); 
            auto it = memberNode->memberList.begin(); 
            size_t remainingEntries = listsize; 
            while (remainingEntries > 0 && it != memberNode->memberList.end()) 
            { 
                bool addOrupdate = true; 
                // Check for valid memory bounds before memcpy 
                if ((ptr + sizeof(MemberListEntry) + sizeof(bool)) <= ((char*)gossip + msgSize)) 
                { 
                memcpy(ptr, &*it, sizeof(MemberListEntry)); 
                memcpy(ptr + sizeof(MemberListEntry), &addOrupdate, sizeof(bool)); 
                ptr += sizeof(MemberListEntry) + sizeof(bool); 
                } 
                else 
                {   // Handle potential memory overrun 
                std::cerr << "Memory bounds exceeded while preparing GOSSIP message" << std::endl; 
                free(gossip); 
                } 
                ++it; 
                --remainingEntries; 
            } 
            sendGossip(gossip, msgSize); 
            free(gossip);
            #ifdef DEBUGLOG
            log->LOG(&memberNode->addr, "Joined the group...");
            #endif            
            break;
        }
         /* node checks the type of the received ping message direct or indirect ping, sends and ack 
         to the sender (SWIM protocol style message exchanging system) */
        case PING: {
            Address SenderAddress;
            memcpy(&SenderAddress.addr, (char *)(msg + 1), sizeof(SenderAddress.addr));
            updateMemberList(msg,size); 
            //extract the flag value
            bool fromPingreq;
            memcpy(&fromPingreq,(char *)(msg + 1)+sizeof(SenderAddress.addr),sizeof(bool));
            if(fromPingreq)
            {
            //indirect PING
            Address ackAddr;
            memcpy(&ackAddr.addr,(char *)(msg + 1)+1+sizeof(SenderAddress.addr), sizeof(ackAddr.addr));
            int countEntries=0;
            for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
            {
              if((par->getcurrtime()-it->timestamp)<=TFAIL)
              ++countEntries;
            }
            size_t flagSize=countEntries*sizeof(char);
            size_t listSize = static_cast<size_t>(countEntries) * sizeof(MemberListEntry);
            //prepare a ACK message
            size_t msgSize=sizeof(MessageHdr)+sizeof(memberNode->addr.addr)*2+listSize+1+flagSize;
            MessageHdr* ack =(MessageHdr*)malloc(msgSize*sizeof(char));//allocation in bytes
            ack ->msgType=ACK;
            bool fromPingreq=true;
            memcpy((char*)(ack +1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
            memcpy((char*)(ack +1)+sizeof(memberNode->addr.addr),&fromPingreq,sizeof(bool));
            memcpy((char*)(ack +1)+sizeof(memberNode->addr.addr)+1,&ackAddr.addr,sizeof(ackAddr.addr));
            char* ptr = (char*)(ack  + 1) + sizeof(memberNode->addr.addr)*2 + sizeof(bool);
            for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
            {
               if ((par->getcurrtime() - it->timestamp) <= TFAIL)
              { 
                bool addOrupdate=true;
                memcpy(ptr, &(*it), sizeof(MemberListEntry));
                memcpy(ptr+sizeof(MemberListEntry),&addOrupdate,sizeof(bool));
                ptr += sizeof(MemberListEntry)+sizeof(bool);
              }        
            }
            emulNet->ENsend(&memberNode->addr, &SenderAddress, (char *)ack, msgSize);
            free(ack);
            }
            else
            {
            //direct PING
            int countEntries=0;
            for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
            {
              if((par->getcurrtime()-it->timestamp)<=TFAIL)
              ++countEntries;
            }
            size_t flagSize=countEntries*sizeof(char);
            size_t listSize = static_cast<size_t>(countEntries) * sizeof(MemberListEntry);
            //prepare a ACK message
            Address dummyAddr;
            dummyAddr.init();    
            size_t msgSize=sizeof(MessageHdr)+sizeof(memberNode->addr.addr)*2+listSize+1+flagSize;
            MessageHdr* ack =(MessageHdr*)malloc(msgSize*sizeof(char));//allocation in bytes
            ack ->msgType=ACK;
            bool fromPingreq=false;
            memcpy((char*)(ack +1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
            memcpy((char*)(ack +1)+sizeof(memberNode->addr.addr),&fromPingreq,sizeof(bool));
            memcpy((char*)(ack +1)+sizeof(memberNode->addr.addr)+1,&dummyAddr.addr,sizeof(dummyAddr.addr));
            char* ptr = (char*)(ack  + 1) + sizeof(memberNode->addr.addr)*2 + sizeof(bool);
            for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
            {
               if ((par->getcurrtime() - it->timestamp) <= TFAIL)
              { 
                bool addOrupdate=true;
                memcpy(ptr, &(*it), sizeof(MemberListEntry));
                memcpy(ptr+sizeof(MemberListEntry),&addOrupdate,sizeof(bool));
                ptr += sizeof(MemberListEntry)+sizeof(bool);
              }        
            }
            emulNet->ENsend(&memberNode->addr, &SenderAddress, (char *)ack, msgSize);
            free(ack);
            }
            break;
        }
         /* node receives acknowledge message, updates its membership list and sends a reply */
        case ACK: {
            Address SenderAddress;//extract the senders address
            memcpy(&SenderAddress.addr, (char *)(msg + 1), sizeof(SenderAddress.addr));
            updateMemberList(msg,size);
            //extract the flag value
            bool fromPingreq;
            memcpy(&fromPingreq,(char *)(msg + 1)+sizeof(SenderAddress.addr),sizeof(bool));
            if(fromPingreq)
            {
            int countEntries=0;
            for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
            {
                if((par->getcurrtime()-it->timestamp)<=TFAIL)
                ++countEntries;
            }
            //prepare an ACK
            size_t flagSize=countEntries*sizeof(char);
            size_t listSize = static_cast<size_t>(countEntries) * sizeof(MemberListEntry);
            Address dummyAddr;
            dummyAddr.init();
            size_t msgSize=sizeof(MessageHdr)+sizeof(memberNode->addr.addr)*2+listSize+1+flagSize;
            MessageHdr* ack =(MessageHdr*)malloc(msgSize*sizeof(char));//allocation in bytes
            ack->msgType=ACK;
            bool fromPingreq=false;
            Address ackAddr;
            memcpy(&ackAddr.addr,(char *)(msg + 1)+sizeof(SenderAddress.addr)+1,sizeof(ackAddr.addr));
            memcpy((char*)(ack+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
            memcpy((char*)(ack+1)+sizeof(memberNode->addr.addr),&fromPingreq,sizeof(bool));
            memcpy((char*)(ack+1)+sizeof(memberNode->addr.addr)+1,&dummyAddr.addr,sizeof(dummyAddr.addr));
            char* ptr = (char*)(ack + 1) + sizeof(memberNode->addr.addr)*2+ sizeof(bool);
            for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
            {
                if ((par->getcurrtime() - it->timestamp) <= TFAIL)
                { 
                    bool addOrupdate=true;
                    memcpy(ptr, &(*it), sizeof(MemberListEntry));
                    memcpy(ptr+sizeof(MemberListEntry),&addOrupdate,sizeof(bool));
                    ptr += sizeof(MemberListEntry)+sizeof(bool);
                }        
            }
            emulNet->ENsend(&memberNode->addr, &ackAddr, (char *)ack, msgSize);
            free(ack);
            }
            break;
        }
         /* node receives ping request message, updates its membership list and sends ping reply message
         (SWIM protocol style message exchange system) */
        case PINGREQ:{
            updateMemberList(msg,size);
            Address SenderAddress;
            memcpy(&SenderAddress.addr, (char *)(msg + 1), sizeof(SenderAddress.addr));
            Address pingAddr;
            memcpy(&pingAddr.addr,(char *)(msg + 1)+1+sizeof(SenderAddress.addr),sizeof(pingAddr.addr));
            int countEntries=0;
            for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
            {
              if((par->getcurrtime()-it->timestamp)<=TFAIL)
              ++countEntries;
            }
            size_t flagSize=countEntries*sizeof(char);
            size_t listSize = static_cast<size_t>(countEntries) * sizeof(MemberListEntry);
            //prepare a PING message
            size_t msgSize=sizeof(MessageHdr)+sizeof(memberNode->addr.addr)*2+listSize+1+flagSize;
            MessageHdr* ping =(MessageHdr*)malloc(msgSize*sizeof(char));//allocation in bytes
            ping->msgType=PING;
            bool fromPingreq=true;
            memcpy((char*)(ping+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
            memcpy((char*)(ping+1)+sizeof(memberNode->addr.addr),&fromPingreq,sizeof(bool));
            memcpy((char*)(ping+1)+sizeof(memberNode->addr.addr)+1,&SenderAddress.addr,sizeof(SenderAddress.addr));
            char* ptr = (char*)(ping + 1) + sizeof(memberNode->addr.addr)*2 + sizeof(bool);
            for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
           {
            if ((par->getcurrtime() - it->timestamp) <= TFAIL)
            { 
               bool addOrupdate=true;
               memcpy(ptr, &(*it), sizeof(MemberListEntry));
               memcpy(ptr+sizeof(MemberListEntry),&addOrupdate,sizeof(bool));
               ptr += sizeof(MemberListEntry)+sizeof(bool);
            }        
           } 
            emulNet->ENsend(&memberNode->addr, &pingAddr, (char *)ping, msgSize);
            free(ping);
            break;
        }
         /* node receives gossip message, updates its membership list and spreads gossip msg 
         if necessary */
        case GOSSIP:{
            //extract time to live value
            int ttl;
            memcpy(&ttl,(char *)(msg + 1),sizeof(int));
            --ttl;
            /* extract memberListEntries and merge into nodes own memberList.
            since the received message structure is different than in most cases, it would take
            extra steps in order to reuse updateMemberList() function, so this message is processed 
            independently */
            size_t myPosIndex = std::distance(memberNode->memberList.begin(), memberNode->myPos);
            char* ptr = (char*)(msg + 1);
            ptr+=sizeof(int);
            while(ptr-(char*)msg<size)
            {
                MemberListEntry sendersEntry;
                memcpy(&sendersEntry, ptr, sizeof(MemberListEntry));
                ptr+=sizeof(MemberListEntry);
                bool addOrupdate;
                memcpy(&addOrupdate,ptr,sizeof(bool));
                ptr += sizeof(bool);
                if(addOrupdate)
                {
                    bool exists = false;
                    for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
                    {
                        //determine if sendersEntry exists in your memberList
                        if(it->id==sendersEntry.id&&it->port==sendersEntry.port)
                        {
                            exists=true;
                            if(sendersEntry.heartbeat>it->heartbeat)
                            {
                                it->setheartbeat(sendersEntry.heartbeat);
                                it->settimestamp(par->getcurrtime());
                            }
                            break;
                        }
                    }
                    if(!exists)
                    {
                    //add a new element to membership list and log it
                    Address logAddr=getAddr(sendersEntry.id,sendersEntry.port);
                    memberNode->memberList.push_back(MemberListEntry(sendersEntry.id,sendersEntry.port,sendersEntry.heartbeat,par->getcurrtime()));
                    #ifdef DEBUGLOG
                    log->logNodeAdd(&memberNode->addr, &logAddr);
                    #endif 
                    memberNode->myPos = memberNode->memberList.begin() + myPosIndex;                   
                    //send GOSSIP with TTL=3 to 2 random nodes
                    int ttl=TTL;
                    bool AddOrUpdate=true;
                    size_t msgSize=sizeof(MessageHdr)+sizeof(int)+sizeof(MemberListEntry)+sizeof(bool);
                    MessageHdr* gossip =(MessageHdr*)malloc(msgSize*sizeof(char));
                    gossip->msgType=GOSSIP;
                    MemberListEntry entry{sendersEntry.id,sendersEntry.port,sendersEntry.heartbeat,par->getcurrtime()};
                    memcpy((char*)(gossip+1), &ttl, sizeof(int));
                    memcpy((char*)(gossip+1)+sizeof(int), &entry, sizeof(MemberListEntry));
                    memcpy((char*)(gossip+1)+sizeof(int)+sizeof(MemberListEntry),&AddOrUpdate,sizeof(bool));
                    sendGossip(gossip,msgSize);
                    free(gossip);
                    }                   

                }
                else
                {   /*
                    //this is executed when the entry needs to be deleted from the list
                    for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end();)
                    { 
                        if (it->id==sendersEntry.id&&it->port==sendersEntry.port)//find the entry
                        {
                        Address removeAddr=getAddr(it->id,it->port);
                        it = memberNode->memberList.erase(it);
                        #ifdef DEBUGLOG
                        log->logNodeRemove(&memberNode->addr, &removeAddr);
                        #endif
                        //send GOSSIP with TTL=3 to 2 random nodes
                        int ttl=TTL;
                        bool AddOrUpdate=false;
                        size_t msgSize=sizeof(MessageHdr)+sizeof(int)+sizeof(MemberListEntry)+sizeof(bool);
                        MessageHdr* gossip =(MessageHdr*)malloc(msgSize*sizeof(char));
                        gossip->msgType=GOSSIP;
                        MemberListEntry entry{sendersEntry.id,sendersEntry.port,sendersEntry.heartbeat,par->getcurrtime()};
                        memcpy((char*)(gossip+1), &ttl, sizeof(int));
                        memcpy((char*)(gossip+1)+sizeof(int), &entry, sizeof(MemberListEntry));
                        memcpy((char*)(gossip+1)+sizeof(int)+sizeof(MemberListEntry),&AddOrUpdate,sizeof(bool));
                        sendGossip(gossip,msgSize); 
                        free(gossip);                  
                        }
                        else 
                        {
                            ++it;
                        }
                    }
                    if (myPosIndex < memberNode->memberList.size()) 
                    { memberNode->myPos = memberNode->memberList.begin() + myPosIndex; } 
                    else { memberNode->myPos = memberNode->memberList.end(); } */
                }           
            }
            if(ttl>0)
            {
                memcpy((char*)(msg+1), &ttl, sizeof(int));
                sendGossip(msg,size);
            }  
            break;
        }
            default:
            break;
}
    return true;
}
/**
 * FUNCTION NAME: sendPingRequest
 *
 * DESCRIPTION: implementation of SWIM style protocol message exchanging system
 */
void MP1Node::sendPingRequest(const MemberListEntry &entry)
{
    Address toAddr;
    memcpy(&toAddr.addr[0], &entry.id, sizeof(int));
	memcpy(&toAddr.addr[4], &entry.port, sizeof(short));
    int countEntries=0;
    for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
    {
        if((par->getcurrtime()-it->timestamp)<=TFAIL)
        ++countEntries;
    }
    size_t flagSize=countEntries*sizeof(char);
    size_t listSize = static_cast<size_t>(countEntries) * sizeof(MemberListEntry);
    //prepare a PING message
    size_t msgSize=sizeof(MessageHdr)+sizeof(memberNode->addr.addr)*2+listSize+1+flagSize;
    MessageHdr* ping =(MessageHdr*)malloc(msgSize*sizeof(char));//allocation in bytes
    ping->msgType=PINGREQ;
    bool fromPingreq=true;
    memcpy((char*)(ping+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
    memcpy((char*)(ping+1)+sizeof(memberNode->addr.addr),&fromPingreq,sizeof(bool));
    memcpy((char*)(ping+1)+sizeof(memberNode->addr.addr)+1,&toAddr.addr,sizeof(toAddr.addr));
    char* ptr = (char*)(ping + 1) + sizeof(memberNode->addr.addr)*2 + sizeof(bool);
    for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
    {
        if ((par->getcurrtime() - it->timestamp) <= TFAIL)
        { 
           bool addOrupdate=true;
           memcpy(ptr, &(*it), sizeof(MemberListEntry));
           memcpy(ptr+sizeof(MemberListEntry),&addOrupdate,sizeof(bool));
           ptr += sizeof(MemberListEntry)+sizeof(bool);
        }        
    }
    int excludeid;
    Address pingAddr;
    for(int i=0; i<2;++i)
    {
        if(i==0)
        {
            pingAddr=getRandomAddress();
            emulNet->ENsend(&memberNode->addr, &pingAddr, (char *)ping, msgSize);
            memcpy(&excludeid, &pingAddr.addr[0],sizeof(int));
        }
        else
        {
            pingAddr=getRandomAddress(excludeid);
            emulNet->ENsend(&memberNode->addr, &pingAddr, (char *)ping, msgSize);
        }
    }
    free(ping);
    return;
}
/**
 * FUNCTION NAME: sendPingRequest
 *
 * DESCRIPTION: implementation of SWIM style protocol message exchanging system
 */
void MP1Node::sendPing()
{
    Address toAddr=getRandomAddress(); 
    //collect recent updates from memberList and piggyback them on PING message
    //calculate the number of entries to include in the message
    int countEntries=0;
    for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
    {
        if((par->getcurrtime()-it->timestamp)<=TFAIL)
        ++countEntries;
    }
    size_t flagSize=countEntries*sizeof(char);
    size_t listSize = static_cast<size_t>(countEntries) * sizeof(MemberListEntry);
    //prepare a PING message
    //the *2 in sizeof(memberNode->addr.addr)*2 is used as a placeholder for this message to
    //be compatible with msgUpdate
    Address dummyAddr;
    dummyAddr.init();    
    size_t msgSize=sizeof(MessageHdr)+sizeof(memberNode->addr.addr)*2+listSize+1+flagSize;
    MessageHdr* ping =(MessageHdr*)malloc(msgSize*sizeof(char));//allocation in bytes
    ping->msgType=PING;
    bool fromPingreq=false;
    memcpy((char*)(ping+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
    memcpy((char*)(ping+1)+sizeof(memberNode->addr.addr),&fromPingreq,sizeof(bool));
    memcpy((char*)(ping+1)+sizeof(memberNode->addr.addr)+1,&dummyAddr.addr,sizeof(dummyAddr.addr));
    char* ptr = (char*)(ping + 1) + sizeof(memberNode->addr.addr)*2 + sizeof(bool);
    for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it)
    {
        if ((par->getcurrtime() - it->timestamp) <= TFAIL)
        { 
           bool addOrupdate=true;
           memcpy(ptr, &(*it), sizeof(MemberListEntry));
           memcpy(ptr+sizeof(MemberListEntry),&addOrupdate,sizeof(bool));
           ptr += sizeof(MemberListEntry)+sizeof(bool);
        }        
    }
    emulNet->ENsend(&memberNode->addr, &toAddr, (char *)ping, msgSize);
    free(ping);
    return;

}
/**
 * FUNCTION NAME: sendPingRequest
 *
 * DESCRIPTION: implementation of gossip style message exchanging system
 */
void MP1Node::sendGossip(MessageHdr*msg, size_t msgSize)
{
    int excludeid;
    Address gossipAddr;
    for(int i=0; i<2;++i)
    {
        if(i==0)
        {
            gossipAddr=getRandomAddress();
            memcpy(&excludeid, &gossipAddr.addr[0],sizeof(int));
            emulNet->ENsend(&memberNode->addr, &gossipAddr, (char *)msg, msgSize);
            
        }
        else
        {
            gossipAddr=getRandomAddress(excludeid);
            emulNet->ENsend(&memberNode->addr, &gossipAddr, (char *)msg, msgSize);
        }
    }
    return;
}
/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {
    //printMemberList("by nodeLoopOps");
	memberNode->heartbeat++;
    //update Node's own entry in MemberList
    memberNode->myPos->heartbeat=memberNode->heartbeat;
    memberNode->myPos->timestamp=par->getcurrtime();
    size_t myPosIndex = std::distance(memberNode->memberList.begin(), memberNode->myPos);
    for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end();)
    { 
        int interval=par->getcurrtime() - it->timestamp;
        //probes every TFAIL time units starting from TFAIL time and not including TREMOVE
        if (it != memberNode->myPos&&interval>=TFAIL&&interval<TREMOVE&&interval%TFAIL==0)
        { 
            sendPingRequest(*it);
            ++it;
            continue;        
        }
        if (it!=memberNode->myPos&&(par->getcurrtime() - it->timestamp) > TREMOVE)
        {
            cout<<"logging memberNode removal from the list..."<<endl;
            Address removeAddr=getAddr(it->id,it->port);
            it = memberNode->memberList.erase(it);
            #ifdef DEBUGLOG
            log->logNodeRemove(&memberNode->addr, &removeAddr);
            #endif                                     
        }
        else 
        {
            // Only increment the iterator if no element is removed
            ++it;
        }           
               
    }
    // Reset myPos to the correct element after modifying the vector 
    if (myPosIndex < memberNode->memberList.size()) 
    { memberNode->myPos = memberNode->memberList.begin() + myPosIndex; } 
    else { memberNode->myPos = memberNode->memberList.end(); } 
    //send PING every TFAIL timeunits
    if (memberNode->pingCounter>0)
    {
        memberNode->pingCounter--;
    } 
    if(memberNode->pingCounter==0)
    {
        sendPing();
        memberNode->pingCounter=TFAIL;
    }
    
    
    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;
    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}
Address MP1Node::getAddr(int id,short port)
{
    Address addr;
    memset(&addr, 0, sizeof(Address));
    *(int *)(&addr.addr) = id;
    *(short *)(&addr.addr[4]) = port;
    return addr;
}
Address MP1Node::getRandomAddress() {
    int myId;
    int toId;
    memcpy(&myId, &memberNode->addr.addr[0], sizeof(int));   
    //check that the randomly selected id is not your id
    bool myAddr=true;
    while(myAddr)
    {
      toId = rand()%(par->EN_GPSZ+1-1)+1;
      if (myId!=toId)
      myAddr=false;
    }
    Address randAddr;
    memset(&randAddr, 0, sizeof(Address));
    *(int *)(&randAddr.addr) = toId;
    *(short *)(&randAddr.addr[4]) = 0;
    return randAddr;    
}
Address MP1Node::getRandomAddress(int excl) {
    int myId;
    int toId;
    memcpy(&myId, &memberNode->addr.addr[0], sizeof(int));   
    //check that the randomly selected id is not your id
    bool myAddr=true;
    while(myAddr)
    {
      toId = rand()%(par->EN_GPSZ+1-1)+1;
      if (myId!=toId&&excl!=toId)
      myAddr=false;
    }
    Address randAddr;
    memset(&randAddr, 0, sizeof(Address));
    *(int *)(&randAddr.addr) = toId;
    *(short *)(&randAddr.addr[4]) = 0;
    return randAddr;    
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode, int id, int port) {
    //In the beginning each node should contain an entry about itself in the MemberList
    long currtime=par->getcurrtime();
    MemberListEntry entry{id,static_cast<short>(port),0,currtime};
    memberNode->memberList.push_back(entry);
    Address addr;
    memset(&addr, 0, sizeof(Address));
    *(int *)(&addr.addr) = id;
    *(short *)(&addr.addr[4]) = port;
    memberNode->myPos = memberNode->memberList.begin();
}
/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}
