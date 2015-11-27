/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/
+#include <sstream>
+
+#ifndef _MP1NODE_H_
#include "MP1Node.h"
+#endif /* _MP1NODE_H_ */

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
        finishUpThisNode(&joinaddr); /khai bao khong hoan thanh thi loai no ra
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
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
    initMemberListTable(memberNode);

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
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr) + 1, &memberNode->heartbeat, sizeof(long));

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
    if(memberNode->inited){
        //free(memberNode->memberList);
        //free(memberNode->mp1q);
       free(memberNode);
    memberNode->inGroup =0;
    }
    return 0;
}


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

    // Do the membership protocol duties

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
    unsigned int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    if (!recvCallBack((void *)memberNode, (char *)ptr, size)) {
#ifdef DEBUGLOG
            log->LOG(&memberNode->addr, "Message handling failed. Exit.");
#endif
        }
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, unsigned int size ) {
    
    // Make sure that you received the right message
    assert(size >= sizeof(MessageHdr));
    
    //Copy the received data after typecasting into MessageHdr type into a local msg
    MessageHdr* msg = (MessageHdr*) data;
    Address *src_addr = (Address*)(msg+1);
    
    size -= sizeof(MessageHdr) + sizeof(Address) + 1;
    data += sizeof(MessageHdr) + sizeof(Address) + 1;
    
    switch (msg->msgType) {
        case JOINREQ:
            JoinHandler(src_addr, data, size);
           HeartbeatHandler(src_addr, data, size);
            break;
        case JOINREP:
        {
            memberNode->inGroup = 1;
            stringstream msg;
            msg << "Received JOINREP from the node " <<  src_addr->getAddress();
            msg << " data " << *(long*)(data );
            log->LOG(&memberNode->addr, msg.str().c_str());
            HeartbeatHandler(src_addr, data, size);
           break;
        }
        case PINGRANDOM:
            HeartbeatHandler(src_addr, data, size);
            break;
           default:
            log->LOG(&memberNode->addr, "Received some invalid message");
            return false;
    }
    return true;
}

Address AddressFromMLE(MemberListEntry* mle) {
    Address a;
    memcpy(a.addr, &mle->id, sizeof(int));
    memcpy(&a.addr[4], &mle->port, sizeof(short));
    return a;
}

/*Handler that can be invoked on reception of a JOINREQ message*/
void MP1Node::JoinHandler(Address* addr, void* data, size_t size) {
    MessageHdr* msg;
    size_t msgsize = sizeof(MessageHdr) + sizeof(memberNode->addr) + sizeof(long) + 1;
    msg = (MessageHdr *) malloc(msgsize * sizeof(char));
    msg->msgType = JOINREP;
    
    memcpy((char *)(msg+1), &memberNode->addr, sizeof(memberNode->addr));
    memcpy((char *)(msg+1) + sizeof(memberNode->addr) + 1, &memberNode->heartbeat, sizeof(long));
    
    stringstream ss;
    ss<< "Sending JOINREP to node" << addr->getAddress() <<" heartbeat "<<memberNode->heartbeat;
    log->LOG(&memberNode->addr, ss.str().c_str());
    emulNet->ENsend(&memberNode->addr, addr, (char *)msg, msgsize);
    free(msg);
}

/*Handler function that can be invoked on reception of a HeartBeat message*/
void MP1Node::HeartbeatHandler(Address* addr, void* data, size_t size) {
    std::stringstream msg;
    assert(size >= sizeof(long));
    long *heartbeat = (long*)data;
    
    bool newData = UpdateMembershipList(addr, *heartbeat);
    if (newData) {
        LogMembershipList();
        SendHBToRandMember(addr, *heartbeat);
    }
}

bool MP1Node::UpdateMembershipList(Address *addr, long heartbeat)  {
    vector<MemberListEntry>::iterator itno;
    for (itno = memberNode->memberList.begin(); itno != memberNode->memberList.end(); itno++) {
        if ((AddressFromMLE(&(*itno)) == *addr) == 0) {
            if (heartbeat > itno->getheartbeat()) {
                itno->setheartbeat(heartbeat);
                itno->settimestamp(par->getcurrtime());
                return true;
            } else {
                return false;
            }
        }
    }
    MemberListEntry mle(*((int*)addr->addr),
                        *((short*)&(addr->addr[4])),
                        heartbeat,
                        par->getcurrtime());
    memberNode->memberList.push_back(mle);
    log->logNodeAdd(&memberNode->addr, addr);
    return true;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

int timeout = 5;
    
    stringstream ss;
    for (vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++) {
        
        if (par->getcurrtime() - it->timestamp > timeout) {
            Address addr = AddressFromMLE(&(*it));
            ss << "Timed out of node " << addr.getAddress();
            log->LOG(&memberNode->addr, ss.str().c_str());
            ss.str("");
            
            vector<MemberListEntry>::iterator next_it = it;
            vector<MemberListEntry>::iterator next_next_it = it+1;
            for (next_it = it; next_next_it != memberNode->memberList.end(); next_it++, next_next_it++) {
                *next_it = *next_next_it;
            }
            memberNode->memberList.resize(memberNode->memberList.size()-1);
            it -= 1;
            LogMembershipList();
            log->logNodeRemove(&memberNode->addr, &addr);
        }
    }
    
    /*Update Membership list based on the changes done above*/
    UpdateMembershipList(&memberNode->addr, ++memberNode->heartbeat);
    
    SendHBToRandMember(&memberNode->addr,
                      memberNode->heartbeat);
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

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode, int id, short port) {
	memberNode->memberList.clear();
	MemberListEntry mle = MemberListEntry(id, port);
    mle.settimestamp(par->getcurrtime());
    mle.setheartbeat(memberNode->heartbeat);
    memberNode->memberList.push_back(mle);
}

void MP1Node::LogMembershipList() {
    stringstream msg;
    msg << "[";
    for (vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++) {
        /*Log the membership list for all the elements of MLE*/
        msg << it->getid() << ": " << it->getheartbeat() << "(" << it->gettimestamp() << "), ";
    }
    msg << "]";
    
}

/*Send ping to random member*/
void MP1Node::SendHBToRandMember(Address *src_addr, long heartbeat) {
 
    double prob = k / (double)memberNode->memberList.size();
    
    MessageHdr *msg;
    
    size_t msgsize = sizeof(MessageHdr) + sizeof(src_addr->addr) + sizeof(long) + 1;
    msg = (MessageHdr *) malloc(msgsize * sizeof(char));
    
   // create JOINREQ message
    msg->msgType = PINGRANDOM;
    memcpy((char *)(msg+1), src_addr->addr, sizeof(src_addr->addr));
    memcpy((char *)(msg+1) + sizeof(src_addr->addr) + 1, &heartbeat, sizeof(long));
    
    for (vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++) {
        Address dst_addr = AddressFromMLE(&(*it));
        
        /*Check the list member is not the member itself*/
        if ((dst_addr == memberNode->addr) == 0 ||
            ((dst_addr == *src_addr) == 0)) {
            continue;
        }
        
        /*Send message based on the probability*/
        if ((((double)(rand() % 100))/100) < prob) {
            emulNet->ENsend(&memberNode->addr, &dst_addr, (char *)msg, msgsize);
        }
    }
    free(msg);
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
+           addr->addr[3], *(short*)&addr->addr[4]) ;        
}
