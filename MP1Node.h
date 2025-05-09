/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Header file of MP1Node class.
 **********************************/

#ifndef _MP1NODE_H_
#define _MP1NODE_H_

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "Member.h"
#include "EmulNet.h"
#include "Queue.h"
#include <iterator>


/**
 * Macros
 */
#define TREMOVE 20
#define TFAIL 5
#define MAX_PARTIAL_LIST_SIZE 10
#define TTL 3

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Message Types
 */
enum MsgTypes{
    JOINREQ,
    JOINREP,
	ACK,
	PING,
	PINGREQ,
	GOSSIP,
    DUMMYLASTMSGTYPE
};

/**
 * STRUCT NAME: MessageStatus
 *
 * DESCRIPTION: Timeout and ack receive status of a message
 */
typedef struct MessageStatus
{
	int id;
	int timeout;
	bool ackReceived;
	bool isActive;
}MessageStatus;

/**
 * STRUCT NAME: MessageHdr
 *
 * DESCRIPTION: Header and content of a message
 */
typedef struct MessageHdr {
	enum MsgTypes msgType;
}MessageHdr;

/**
 * CLASS NAME: MP1Node
 *
 * DESCRIPTION: Class implementing Membership protocol functionalities for failure detection
 */
class MP1Node {
private:
	EmulNet *emulNet;
	Log *log;
	Params *par;
	Member *memberNode;
	char NULLADDR[6];
public:
	MP1Node(Member *, Params *, EmulNet *, Log *, Address *);
	Member * getMemberNode() {
		return memberNode;
	}
	int recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);
	void nodeStart(char *servaddrstr, short serverport);
	void printMemberList(const char*location);
	int initThisNode(Address *joinaddr);
	int introduceSelfToGroup(Address *joinAddress);
	int finishUpThisNode();
	bool ackreceived=false;
	void nodeLoop();
	void checkMessages();
	void updateMemberList(MessageHdr*msg);
	void updateMemberList(MessageHdr*msg,int size);
	bool recvCallBack(void *env, char *data, int size);
	void sendPing();
	void sendPingRequest(const MemberListEntry &entry);
	void sendGossip(MessageHdr*msg,size_t msgSize);
	void nodeLoopOps();
	int isNullAddress(Address *addr);
	Address getJoinAddress();
	Address getAddr(int id,short port);
	Address getRandomAddress();
	Address getRandomAddress(int excl);
	void initMemberListTable(Member *memberNode, int id, int port);
	void printAddress(Address *addr);
	virtual ~MP1Node();
};

#endif /* _MP1NODE_H_ */
