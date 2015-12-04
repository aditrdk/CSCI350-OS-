#include "copyright.h"

#include "system.h"
#include "network.h"
#include "post.h"
#include "interrupt.h"
#include "syscall.h"
#include <sstream>

PacketHeader outPktHdr, inPktHdr;
MailHeader outMailHdr, inMailHdr;

enum State {AVAIL, BUSY};

struct ServerLock{
	char  name[MaxMailSize];
	int machineID;
	int port;
	State state;
	List* queue;
	bool isToBeDeleted;
};

struct ServerCondition{
	char name[MaxMailSize];
	int lockIndex;
	List* queue;
	bool isToBeDeleted;
};

struct ServerMonitor{
	char name[MaxMailSize];
	int value;
};

struct PendingRequest{
	char requestType[MaxMailSize];
	char name[MaxMailSize];
	char call[MaxMailSize];
	int numNoResponses;
	int mailbox;
	int machineId;
};

ServerLock* serverlockTable;
BitMap serverLockMap(numLocks);

ServerCondition* serverconditionTable;
BitMap serverConditionMap(numConditions);

int numMonitors = numLocks;
ServerMonitor* servermonitorTable;
BitMap serverMonitorMap(numMonitors);

int numRequests = 500;
PendingRequest *pendingrequestTable;
BitMap pendingrequestMap(numRequests);

void ErrorMessage(char emessage[]){
	char message[MaxMailSize];
	strcpy(message, "Error ");
	strcat(message, emessage);
	outPktHdr.to = inPktHdr.from;
   	outMailHdr.to = inMailHdr.from;
   	outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);
}

int CheckLockTable(char name[]){
	for(int i = 0; i < numLocks; i++){
      if(serverLockMap.Test(i)){
        if(!strcmp(serverlockTable[i].name, name)){
			return i;
        }
      }
    }
    return -1;
}

int CheckConditionTable(char name[]){
	for(int i = 0; i < numConditions; i++){
      if(serverConditionMap.Test(i)){
        if(!strcmp(serverconditionTable[i].name, name)){
			return i;
        }
      }
    }
    return -1;
}

int CheckMonitorTable(char name[]){
	for(int i = 0; i < numMonitors; i++){
      if(serverMonitorMap.Test(i)){
        if(!strcmp(servermonitorTable[i].name, name)){
			return i;
        }
      }
    }
    return -1;
}

int ValidateLockNum(int lockNum){
	if(lockNum < 0 || lockNum >= numLocks){
		printf("invalid lock index %d\n", lockNum);
		return 0;
	} 
	if(!serverLockMap.Test(lockNum)){
		printf("lock is not currently assigned.%d\n", lockNum);
		return 0;
	} 

	return 1;
}

bool VerifyLock(int lockNum, int mID, int mBox){
	if(!ValidateLockNum(lockNum)) return false;
	return (serverlockTable[lockNum].machineID == mID && serverlockTable[lockNum].port == mBox);
}


int ValidateConditionNum(int conditionNum){
	if(conditionNum < 0 || conditionNum >= numConditions){
		printf("invalid condition index %d\n", conditionNum);
		return 0;
	} 
	if(!serverConditionMap.Test(conditionNum)){
		printf("condition not currently assigned %d\n", conditionNum);
		return 0;
	} 
	return 1;
}

int ValidateMonitorNum(int monitorNum){
	if(monitorNum < 0 || monitorNum >= numMonitors){
		printf("invalid monitor index %d\n", monitorNum);
		return 0;
	} 
	if(!serverMonitorMap.Test(monitorNum)){
		printf("monitor not currently assigned %d\n", monitorNum);
		return 0;
	} 
	return 1;
}

void SendMessage(char m[MaxMailSize], int mID = inPktHdr.from, int mBox = inMailHdr.from){
    char message[MaxMailSize];
    strcpy(message, m);
    outPktHdr.to = mID;
	outMailHdr.to = mBox;
 	outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);
}

void AppendReplyData(char message[MaxMailSize], int index, int mID = inPktHdr.from, int mBox = inMailHdr.from){
	char address[MaxMailSize], indexString[MaxMailSize];
    strcat(message, " ");
	int returnData;
	returnData = mID * 1000;
	returnData += mBox;
	std::stringstream rs; 
	rs << returnData;
	rs >> address;
	strcat(message, address);
	strcat(message, " ");
	std::stringstream ss;
	ss << index;
	ss >> indexString;
	strcat(message, indexString);
}

void CreateLock(char name[], int mID, int mBox){
	char message[MaxMailSize];
	std::stringstream ss;
	strcpy(message, "LockNum ");
	char lockNum[MaxMailSize];
    int index = serverLockMap.Find();
    if(index != -1) {
      printf("Creating New lock\n");
      strcpy(serverlockTable[index].name, name);
      serverlockTable[index].state = AVAIL;
      serverlockTable[index].isToBeDeleted = false;
      serverlockTable[index].queue = new List;
      index += machineId*1000;
      ss << index;
      ss >> lockNum;
      strcat(message, lockNum);
      printf("SS'd and cat'd\n");
      printf("Initialized serverlock\n");
    }
    else {
      printf("No empty locks in lockTable\n");
      strcpy(message, "TableFull");
    }
	SendMessage(message, mID, mBox);
}

void BroadcastToServers(char message[MaxMailSize]){
	for(int i = 0; i < numServers; i++){
		if(i != machineId){
			SendMessage(message, i, 0);
		}
	}
}

void ServerCreateLock(char name[]){
	char message[MaxMailSize];
	std::stringstream ss;
	strcpy(message, "LockNum ");
	char lockNum[MaxMailSize];
	int id = CheckLockTable(name);
	if(id != -1){
		id += machineId*1000;
        ss << id;
    	ss >> lockNum;
    	strcat(message, lockNum);
    	SendMessage(message);
		return;
	}
    else{
    	int index = pendingrequestMap.Find();
    	if(index != -1){
    		strcpy(pendingrequestTable[index].requestType, "CL");
    		strcpy(pendingrequestTable[index].name, name);
    		pendingrequestTable[index].numNoResponses = 0;
    		pendingrequestTable[index].mailbox = inMailHdr.from;
    		pendingrequestTable[index].machineId = inPktHdr.from;
	    	strcpy(message, "Server ");
	    	strcat(message, "CL ");
	    	strcat(message, name);
	    	AppendReplyData(message, index);
	    	printf("%s\n", message);
	    	BroadcastToServers(message);
	    	if(numServers == 1){
	    		CreateLock(name, inPktHdr.from, inMailHdr.from);
	    	}
		}
		else {
	      printf("No empty requests in requestTable\n");
	      strcpy(message, "TableFull");
	      SendMessage(message);
	    }
    }
}

void AcquireLock(int lockNum, int mID, int mBox){
	char message[MaxMailSize];
	if(!ValidateLockNum(lockNum) || serverlockTable[lockNum].isToBeDeleted){
		strcpy(message, "InvalidLock");
		SendMessage(message, mID, mBox);
	}
	if(serverlockTable[lockNum].state == AVAIL){
		serverlockTable[lockNum].state = BUSY;
		serverlockTable[lockNum].machineID = mID;
		serverlockTable[lockNum].port = mBox;
		strcpy(message, "AcquiredLock ");
		std::stringstream ls;
		ls << lockNum;
		char index[MaxMailSize];
		ls >> index;
		strcat(message, index);
		SendMessage(message, mID, mBox);
	}
	else if(serverlockTable[lockNum].machineID == mID && serverlockTable[lockNum].port == mBox){
		strcpy(message, "AcquiredLock ");
		std::stringstream ls;
		ls << lockNum;
		char index[MaxMailSize];
		ls >> index;
		strcat(message, index);
		SendMessage(message, mID, mBox);
	}
	else {
		int returnData;
		returnData = mID * 1000;
		returnData += mBox;
		printf("Appending to queue\n");
		serverlockTable[lockNum].queue->Append((void*)returnData);
	}
}

void ServerAcquireLock(char* data){
	int lockNum;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> lockNum;
	if(lockNum/1000 == machineId){
		AcquireLock(lockNum%1000, inPktHdr.from, inMailHdr.from);
	}
	else{
    	int index = pendingrequestMap.Find();
    	if(index != -1){
    		strcpy(pendingrequestTable[index].requestType, "AL");
    		pendingrequestTable[index].numNoResponses = 0;
    		pendingrequestTable[index].mailbox = inMailHdr.from;
    		pendingrequestTable[index].machineId = inPktHdr.from;
			strcpy(message, "Server ");
			strcat(message, "AL ");
			strcat(message, data);
			AppendReplyData(message, index);
	    	printf("%s\n", message);
	    	BroadcastToServers(message);
	    	if(numServers == 1){
	    		printf("Trying to acquire a lock that is not currently assigned.%d\n", lockNum);
				strcpy(message, "InvalidLock");
				SendMessage(message);
	    	}
		}
		else {
	      printf("No empty requests in requestTable\n");
	      strcpy(message, "TableFull");
	      SendMessage(message);
	    }
	}
}

void ReleaseWaitingClient(int lockNum){
	char message[MaxMailSize];
	printf("Releasing lock %d\n", lockNum);
	if(serverlockTable[lockNum].queue->IsEmpty()){
		serverlockTable[lockNum].state = AVAIL;
	}
	else{
		strcpy(message, "AcquiredLock ");
		int machineID, port;
		std::stringstream ls;
		ls << lockNum;
		char index[MaxMailSize];
		ls >> index;
		strcat(message, index);
		int messageInfo;
		messageInfo = (int)serverlockTable[lockNum].queue->Remove();
		printf("Message Info %d\n", messageInfo);
		machineID = messageInfo/1000;
		port = messageInfo % 100;
		printf("Going to send message [%s] to machineID %d, port %d\n", message, machineID, port);
		serverlockTable[lockNum].state = BUSY;
		serverlockTable[lockNum].machineID = machineID;
		serverlockTable[lockNum].port = port;
		SendMessage(message, machineID, port);
	}
}

void ReleaseLock(int lockNum, int mID, int mBox){
	char message[MaxMailSize];
	if(!ValidateLockNum(lockNum)){
		strcpy(message, "InvalidLock");
		SendMessage(message, mID, mBox);
	}
	if(!(serverlockTable[lockNum].machineID == mID && serverlockTable[lockNum].port == mBox)){
		printf("Cannot release lock. Current thread is not holding the lock %d\n", lockNum);
		strcpy(message, "InvalidLock");
		SendMessage(message, mID, mBox);
		return;
	}
	ReleaseWaitingClient(lockNum);
	strcpy(message, "ReleasedLock");
	SendMessage(message, mID, mBox);
	if(serverlockTable[lockNum].isToBeDeleted && serverlockTable[lockNum].queue->IsEmpty()){
		printf("Destroying lock %d\n", lockNum);
		delete serverlockTable[lockNum].queue;
		serverlockTable[lockNum].isToBeDeleted = false;
		serverLockMap.Clear(lockNum);
	}
}

void ServerReleaseLock(char* data){
	int lockNum;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> lockNum;
	if(lockNum/1000 == machineId){
		ReleaseLock(lockNum%1000, inPktHdr.from, inMailHdr.from);
	}
	else{
    	int index = pendingrequestMap.Find();
    	if(index != -1){
    		strcpy(pendingrequestTable[index].requestType, "RL");
    		pendingrequestTable[index].numNoResponses = 0;
    		pendingrequestTable[index].mailbox = inMailHdr.from;
    		pendingrequestTable[index].machineId = inPktHdr.from;
			strcpy(message, "Server ");
			strcat(message, "RL ");
			strcat(message, data);
			AppendReplyData(message, index);
	    	printf("%s\n", message);
	    	BroadcastToServers(message);
	    	if(numServers == 1){
	    		printf("Trying to release a lock that is not currently assigned.%d\n", lockNum);
				strcpy(message, "InvalidLock");
				SendMessage(message);
	    	}
		}
		else {
	      printf("No empty requests in requestTable\n");
	      strcpy(message, "TableFull");
	      SendMessage(message);
	    }
	}

}

void SignalCondition(int conditionNum, int lockNum, int mID, int mBox){
	char message[MaxMailSize];
	if(!ValidateConditionNum(conditionNum)){
		strcpy(message, "InvalidCondition");
		SendMessage(message, mID, mBox);
	}
	if(serverconditionTable[conditionNum].lockIndex != lockNum){
		printf("Condition Lock %d does not match Waiting Lock %d.\n", lockNum, serverconditionTable[conditionNum].lockIndex);
		strcpy(message, "InvalidLock");
		SendMessage(message, mID, mBox);
		return;
	}
	if(serverconditionTable[conditionNum].queue->IsEmpty()){
		strcpy(message, "SignalledCondition");
		SendMessage(message, mID, mBox);
		return;
	}
	strcpy(message, "WaitedCondition ");
	int machineID, port;
	std::stringstream ls;
	ls << conditionNum;
	char index[MaxMailSize];
	ls >> index;
	strcat(message, index);
	int messageInfo;
	messageInfo = (int)serverconditionTable[conditionNum].queue->Remove();
	printf("Message Info %d\n", messageInfo);
	machineID = messageInfo/1000;
	port = messageInfo % 100;
	printf("Going to send message to machineID %d, port %d\n", machineID, port);
	SendMessage(message, machineID, port);	
	if(serverconditionTable[conditionNum].queue->IsEmpty()){
		serverconditionTable[conditionNum].lockIndex = -1;
	}
	strcpy(message, "SignalledCondition");
	SendMessage(message, mID, mBox);
	if(serverconditionTable[conditionNum].isToBeDeleted && serverconditionTable[conditionNum].queue->IsEmpty()){
		printf("Destroying condition %d\n", conditionNum);
		delete serverconditionTable[conditionNum].queue;
		serverconditionTable[conditionNum].isToBeDeleted = false;
		serverConditionMap.Clear(conditionNum);
	}
}

void ServerSignalCondition(char* data){
	int lockNum, conditionNum;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> conditionNum;
	ss >> lockNum;
	if(lockNum/1000 == machineId && conditionNum/1000 == machineId){
		if(VerifyLock(lockNum%1000, inPktHdr.from, inMailHdr.from)){
			SignalCondition(conditionNum%1000, lockNum, inPktHdr.from, inMailHdr.from);
		}
		else{
			strcpy(message, "InvalidCondition");
			SendMessage(message);
		}
	} 	
	else{
		if(lockNum/1000 == machineId){
			if(!VerifyLock(lockNum%1000, inPktHdr.from, inMailHdr.from)){
				strcpy(message, "InvalidLock");
				SendMessage(message);
				return;
			}
			int index = pendingrequestMap.Find();
	    	if(index != -1){
	    		strcpy(pendingrequestTable[index].requestType, "SC");
	    		pendingrequestTable[index].numNoResponses = 0;
	    		pendingrequestTable[index].mailbox = inMailHdr.from;
	    		pendingrequestTable[index].machineId = inPktHdr.from;
				strcpy(message, "Server ");
				strcat(message, "SC ");
				strcat(message, data);
				AppendReplyData(message, index);
		    	printf("%s\n", message);
		    	BroadcastToServers(message);
		    	if(numServers == 1){
		    		//printf("Trying to release a lock that is not currently assigned.%d\n", lockNum);
					strcpy(message, "InvalidCondition");
					SendMessage(message);
		    	}
			}
			else {
		      printf("No empty requests in requestTable\n");
		      strcpy(message, "TableFull");
		      SendMessage(message);
		    }
		}
		else{
			int index = pendingrequestMap.Find();
	    	if(index != -1){
	    		strcpy(pendingrequestTable[index].requestType, "VL");
	    		strcpy(pendingrequestTable[index].name, data);
	    		strcpy(pendingrequestTable[index].call, "SC");
	    		pendingrequestTable[index].numNoResponses = 0;
	    		pendingrequestTable[index].mailbox = inMailHdr.from;
	    		pendingrequestTable[index].machineId = inPktHdr.from;
				strcpy(message, "Server ");
				strcat(message, "VL ");
				strcat(message, data);
				AppendReplyData(message, index);
		    	printf("%s\n", message);
		    	BroadcastToServers(message);
		    	if(numServers == 1){
		    		//printf("Trying to release a lock that is not currently assigned.%d\n", lockNum);
					strcpy(message, "InvalidCondition");
					SendMessage(message);
		    	}
			}
			else {
		      printf("No empty requests in requestTable\n");
		      strcpy(message, "TableFull");
		      SendMessage(message);
		    }
		}
	}
}

void BroadcastCondition(int conditionNum, int lockNum, int mID, int mBox){
	char message[MaxMailSize];
	if(!ValidateConditionNum(conditionNum)){
		strcpy(message, "InvalidCondition");
		SendMessage(message, mID, mBox);
	}
	if(serverconditionTable[conditionNum].lockIndex != lockNum){
		printf("Condition Lock %d does not match Waiting Lock %d.\n", lockNum, serverconditionTable[conditionNum].lockIndex);
		strcpy(message, "InvalidLock");
		SendMessage(message, mID, mBox);
		return;
	}
	while(!serverconditionTable[conditionNum].queue->IsEmpty()){
		SignalCondition(conditionNum, lockNum, mID, mBox);
	}
	strcpy(message, "BroadcastedCondition");
	SendMessage(message, mID, mBox);
	if(serverconditionTable[conditionNum].isToBeDeleted && serverconditionTable[conditionNum].queue->IsEmpty()){
		printf("Destroying condition %d\n", conditionNum);
		delete serverconditionTable[conditionNum].queue;
		serverconditionTable[conditionNum].isToBeDeleted = false;
		serverConditionMap.Clear(conditionNum);
	}
}

void ServerBroadcastCondition(char* data){
	int lockNum, conditionNum;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> conditionNum;
	ss >> lockNum;
	if(lockNum/1000 == machineId && conditionNum/1000 == machineId){
		if(VerifyLock(lockNum%1000, inPktHdr.from, inMailHdr.from)){
			SignalCondition(conditionNum%1000, lockNum, inPktHdr.from, inMailHdr.from);
		}
		else{
			strcpy(message, "InvalidCondition");
			SendMessage(message);
		}
	} 	
	else{
		if(lockNum/1000 == machineId){
			if(!VerifyLock(lockNum%1000, inPktHdr.from, inMailHdr.from)){
				strcpy(message, "InvalidLock");
				SendMessage(message);
				return;
			}
			int index = pendingrequestMap.Find();
	    	if(index != -1){
	    		strcpy(pendingrequestTable[index].requestType, "BC");
	    		pendingrequestTable[index].numNoResponses = 0;
	    		pendingrequestTable[index].mailbox = inMailHdr.from;
	    		pendingrequestTable[index].machineId = inPktHdr.from;
				strcpy(message, "Server ");
				strcat(message, "BC ");
				strcat(message, data);
				AppendReplyData(message, index);
		    	printf("%s\n", message);
		    	BroadcastToServers(message);
		    	if(numServers == 1){
		    		//printf("Trying to release a lock that is not currently assigned.%d\n", lockNum);
					strcpy(message, "InvalidCondition");
					SendMessage(message);
		    	}
			}
			else {
		      printf("No empty requests in requestTable\n");
		      strcpy(message, "TableFull");
		      SendMessage(message);
		    }
		}
		else{
			int index = pendingrequestMap.Find();
	    	if(index != -1){
	    		strcpy(pendingrequestTable[index].requestType, "VL");
	    		strcpy(pendingrequestTable[index].name, data);
	    		strcpy(pendingrequestTable[index].call, "BC");
	    		pendingrequestTable[index].numNoResponses = 0;
	    		pendingrequestTable[index].mailbox = inMailHdr.from;
	    		pendingrequestTable[index].machineId = inPktHdr.from;
				strcpy(message, "Server ");
				strcat(message, "VL ");
				strcat(message, data);
				AppendReplyData(message, index);
		    	printf("%s\n", message);
		    	BroadcastToServers(message);
		    	if(numServers == 1){
		    		//printf("Trying to release a lock that is not currently assigned.%d\n", lockNum);
					strcpy(message, "InvalidCondition");
					SendMessage(message);
		    	}
			}
			else {
		      printf("No empty requests in requestTable\n");
		      strcpy(message, "TableFull");
		      SendMessage(message);
		    }
		}
	}
}

void ServerDestroyLock(char* data){
	int lockNum;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> lockNum;
	if(lockNum < 0 || lockNum >= numLocks){
		printf("Trying to release invalid lock index %d\n", lockNum);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	} 
	if(!serverLockMap.Test(lockNum)){
		printf("Trying to release a lock that is not currently assigned.%d\n", lockNum);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	} 
	if(serverlockTable[lockNum].state == BUSY || !serverlockTable[lockNum].queue->IsEmpty()){
		printf("Marking lock %d\n", lockNum);
		serverlockTable[lockNum].isToBeDeleted = true;
	}
	else{
		printf("Destroying lock %d\n", lockNum);
		delete serverlockTable[lockNum].queue;
		serverlockTable[lockNum].isToBeDeleted = false;
		serverLockMap.Clear(lockNum);
	}
	strcpy(message, "DestroyedLock");
	outPktHdr.to = inPktHdr.from;
    outMailHdr.to = inMailHdr.from;
   	outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);
}

void ServerDestroyCondition(char* data){
	int conditionNum;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> conditionNum;
	if(conditionNum < 0 || conditionNum >= numConditions){
		printf("invalid condition index %d\n", conditionNum);
		strcpy(message, "InvalidCondition");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	} 
	if(!serverConditionMap.Test(conditionNum)){
		printf("condition not currently assigned %d\n", conditionNum);
		strcpy(message, "InvalidCondition");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}
	if(!serverconditionTable[conditionNum].queue->IsEmpty()){
		printf("Marking condition %d\n", conditionNum);
		serverconditionTable[conditionNum].isToBeDeleted = true;
	}
	else{
		printf("Destroying condition %d\n", conditionNum);
		delete serverconditionTable[conditionNum].queue;
		serverconditionTable[conditionNum].isToBeDeleted = false;
		serverConditionMap.Clear(conditionNum);
	}
	strcpy(message, "DestroyedCondition");
	outPktHdr.to = inPktHdr.from;
    outMailHdr.to = inMailHdr.from;
   	outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);
}

void CreateCondition(char name[], int mID, int mBox){
	char message[MaxMailSize];
	std::stringstream ss;
	strcpy(message, "ConditionNum ");
	char conditionNum[MaxMailSize];
    int index = serverConditionMap.Find();
    if(index != -1) {
      printf("Creating New condition\n");
      strcpy(serverconditionTable[index].name, name);
      serverconditionTable[index].isToBeDeleted = false;
      serverconditionTable[index].lockIndex = -1;
      serverconditionTable[index].queue = new List;
      index += machineId*1000;
      ss << index;
      ss >> conditionNum;
      strcat(message, conditionNum);
      printf("SS'd and cat'd\n");
      printf("Initialized servercondition\n");
    }
    else {
      printf("No empty conditions in conditionTable\n");
      strcpy(message, "TableFull");
    }
	SendMessage(message, mID, mBox);
}

void ServerCreateCondition(char name[]){
	char message[MaxMailSize], conditionNum[MaxMailSize];
	std::stringstream ss;
	strcpy(message, "ConditionNum ");
	int id = CheckConditionTable(name);
	if(id != -1){
        	id += machineId*1000;
        	ss << id;
        	ss >> conditionNum;
        	strcat(message, conditionNum);
          	SendMessage(message);
			return;
	}
    else{
    	int index = pendingrequestMap.Find();
    	if(index != -1){
    		strcpy(pendingrequestTable[index].requestType, "CC");
    		strcpy(pendingrequestTable[index].name, name);
    		pendingrequestTable[index].numNoResponses = 0;
    		pendingrequestTable[index].mailbox = inMailHdr.from;
    		pendingrequestTable[index].machineId = inPktHdr.from;
	    	strcpy(message, "Server ");
	    	strcat(message, "CC ");
	    	strcat(message, name);
	    	AppendReplyData(message, index);
	    	printf("%s\n", message);
	    	BroadcastToServers(message);
	    	if(numServers == 1){
	    		CreateCondition(name, inPktHdr.from, inMailHdr.from);
	    	}
		}
		else {
	      printf("No empty requests in requestTable\n");
	      strcpy(message, "TableFull");
	      SendMessage(message);
	    }
    }
}

void ReleaseLockOnWait(int lockNum){
	char index[MaxMailSize], message[MaxMailSize];
	std::stringstream ss;
	ss << lockNum;
	ss >> index;
	strcpy(message, "Server ");
	strcat(message, "RWC ");
	strcat(message, index);
	printf("%s\n", message);
	for(int i = 0; i < numServers; i++){
		SendMessage(message, i, 0);
	}
}

void WaitCondition(int conditionNum, int lockNum, int mID, int mBox){
	char message[MaxMailSize];
	if(!ValidateConditionNum(conditionNum)){
		strcpy(message, "InvalidCondition");
		SendMessage(message, mID, mBox);
	}
	if(serverconditionTable[conditionNum].isToBeDeleted){
		printf("Cannot wait on condition. Condition is marked to be deleted.%d\n", lockNum);
		strcpy(message, "InvalidCondition");
		SendMessage(message, mID, mBox);
		return;
	}
	if(serverconditionTable[conditionNum].lockIndex == -1){
		serverconditionTable[conditionNum].lockIndex = lockNum;
	}
	else if(serverconditionTable[conditionNum].lockIndex != lockNum){
		printf("Condition Lock %d does not match Waiting Lock %d.\n", lockNum, serverconditionTable[conditionNum].lockIndex);
		strcpy(message, "InvalidLock");
		SendMessage(message, mID, mBox);
		return;
	}
	int returnData;
	returnData = mID * 1000;
	returnData += mBox;
	printf("Appending to queue\n");
	serverconditionTable[conditionNum].queue->Append((void*)returnData);
	ReleaseLockOnWait(lockNum);
}

void ServerWaitCondition(char* data){
	int lockNum, conditionNum;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> conditionNum;
	ss >> lockNum;
	if(lockNum/1000 == machineId && conditionNum/1000 == machineId){
		if(VerifyLock(lockNum%1000, inPktHdr.from, inMailHdr.from)){
			WaitCondition(conditionNum%1000, lockNum, inPktHdr.from, inMailHdr.from);
		}
		else{
			strcpy(message, "InvalidCondition");
			SendMessage(message);
		}
	} 	
	else{
		if(lockNum/1000 == machineId){
			if(!VerifyLock(lockNum%1000, inPktHdr.from, inMailHdr.from)){
				strcpy(message, "InvalidLock");
				SendMessage(message);
				return;
			}
			int index = pendingrequestMap.Find();
	    	if(index != -1){
	    		strcpy(pendingrequestTable[index].requestType, "WC");
	    		pendingrequestTable[index].numNoResponses = 0;
	    		pendingrequestTable[index].mailbox = inMailHdr.from;
	    		pendingrequestTable[index].machineId = inPktHdr.from;
				strcpy(message, "Server ");
				strcat(message, "WC ");
				strcat(message, data);
				AppendReplyData(message, index);
		    	printf("%s\n", message);
		    	BroadcastToServers(message);
		    	if(numServers == 1){
		    		//printf("Trying to release a lock that is not currently assigned.%d\n", lockNum);
					strcpy(message, "InvalidCondition");
					SendMessage(message);
		    	}
			}
			else {
		      printf("No empty requests in requestTable\n");
		      strcpy(message, "TableFull");
		      SendMessage(message);
		    }
		}
		else{
			int index = pendingrequestMap.Find();
	    	if(index != -1){
	    		strcpy(pendingrequestTable[index].requestType, "VL");
	    		strcpy(pendingrequestTable[index].name, data);
	    		strcpy(pendingrequestTable[index].call, "WC");
	    		pendingrequestTable[index].numNoResponses = 0;
	    		pendingrequestTable[index].mailbox = inMailHdr.from;
	    		pendingrequestTable[index].machineId = inPktHdr.from;
				strcpy(message, "Server ");
				strcat(message, "VL ");
				strcat(message, data);
				AppendReplyData(message, index);
		    	printf("%s\n", message);
		    	BroadcastToServers(message);
		    	if(numServers == 1){
		    		//printf("Trying to release a lock that is not currently assigned.%d\n", lockNum);
					strcpy(message, "InvalidCondition");
					SendMessage(message);
		    	}
			}
			else {
		      printf("No empty requests in requestTable\n");
		      strcpy(message, "TableFull");
		      SendMessage(message);
		    }
		}
	}
}

void CreateMonitor(char name[], int mID, int mBox){
	char message[MaxMailSize];
	std::stringstream ss;
	strcpy(message, "MonitorNum ");
	char monitorNum[MaxMailSize];
	int index = serverMonitorMap.Find();
    if(index != -1) {
      printf("Creating New monitor\n");
      strcpy(servermonitorTable[index].name, name);
      servermonitorTable[index].value = -1;
      index += machineId*1000;
      ss << index;
      ss >> monitorNum;
      strcat(message, monitorNum);
      printf("SS'd and cat'd\n");
      printf("Initialized servermonitor\n");
    }
    else {
      printf("No empty monitors in monitorTable\n");
      strcpy(message, "TableFull");
    }
    SendMessage(message, mID, mBox);
}

void ServerCreateMonitor(char name[]){
	char message[MaxMailSize];
	std::stringstream ss;
	strcpy(message, "MonitorNum ");
	char monitorNum[MaxMailSize];
	int id = CheckMonitorTable(name);
	if(id != -1){
    	id += machineId*1000;
    	ss << id;
    	ss >> monitorNum;
    	strcat(message, monitorNum);
      	SendMessage(message);
		return;
  	}
  	else{
    	int index = pendingrequestMap.Find();
    	if(index != -1){
    		strcpy(pendingrequestTable[index].requestType, "CM");
    		strcpy(pendingrequestTable[index].name, name);
    		pendingrequestTable[index].numNoResponses = 0;
    		pendingrequestTable[index].mailbox = inMailHdr.from;
    		pendingrequestTable[index].machineId = inPktHdr.from;
	    	strcpy(message, "Server ");
	    	strcat(message, "CM ");
	    	strcat(message, name);
	    	AppendReplyData(message, index);
	    	printf("%s\n", message);
	    	BroadcastToServers(message);
	    	if(numServers == 1){
	    		CreateMonitor(name, inPktHdr.from, inMailHdr.from);
	    	}
		}
		else {
	      printf("No empty requests in requestTable\n");
	      strcpy(message, "TableFull");
	      SendMessage(message);
	    }
    }
}

void SetMonitor(int monitorNum, int value, int mID, int mBox){
	char message[MaxMailSize];
	if(!ValidateMonitorNum(monitorNum)){
		strcpy(message, "InvalidMonitor");
		SendMessage(message, mID, mBox);
	}
	servermonitorTable[monitorNum].value = value;
	printf("Monitor %d has a value of %d",monitorNum, servermonitorTable[monitorNum].value);
	strcpy(message, "SetMonitor");
	SendMessage(message, mID, mBox);
}

void ServerSetMonitor(char * data){
	int monitorNum, value;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> monitorNum;
	ss >> value;
	if(monitorNum/1000 == machineId){
		SetMonitor(monitorNum%1000, value, inPktHdr.from, inMailHdr.from);
	}
	else{
    	int index = pendingrequestMap.Find();
    	if(index != -1){
    		strcpy(pendingrequestTable[index].requestType, "SM");
    		pendingrequestTable[index].numNoResponses = 0;
    		pendingrequestTable[index].mailbox = inMailHdr.from;
    		pendingrequestTable[index].machineId = inPktHdr.from;
			strcpy(message, "Server ");
			strcat(message, "SM ");
			strcat(message, data);
			AppendReplyData(message, index);
	    	printf("%s\n", message);
	    	BroadcastToServers(message);
	    	if(numServers == 1){
	    		printf("Trying to set a monitor that is not currently assigned.%d\n", monitorNum);
				strcpy(message, "InvalidMonitor");
				SendMessage(message);
	    	}
		}
		else {
	      printf("No empty requests in requestTable\n");
	      strcpy(message, "TableFull");
	      SendMessage(message);
	    }
	}
}

void GetMonitor(int monitorNum, int mID, int mBox){
	char message[MaxMailSize];
	if(!ValidateMonitorNum(monitorNum)){
		strcpy(message, "InvalidMonitor");
		SendMessage(message, mID, mBox);
	}
	strcpy(message, "GotMonitor ");
	char value[MaxMailSize];
	std::stringstream bs;
	bs << servermonitorTable[monitorNum].value;
	bs >> value;
	printf("Monitor %d has a value of %s\n",monitorNum, value);
	strcat(message, value);
	SendMessage(message, mID, mBox);
}

void ServerGetMonitor(char* data){
	int monitorNum;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> monitorNum;
	if(monitorNum/1000 == machineId){
		GetMonitor(monitorNum%1000, inPktHdr.from, inMailHdr.from);
	}
	else{
    	int index = pendingrequestMap.Find();
    	if(index != -1){
    		strcpy(pendingrequestTable[index].requestType, "GM");
    		pendingrequestTable[index].numNoResponses = 0;
    		pendingrequestTable[index].mailbox = inMailHdr.from;
    		pendingrequestTable[index].machineId = inPktHdr.from;
			strcpy(message, "Server ");
			strcat(message, "GM ");
			strcat(message, data);
			AppendReplyData(message, index);
	    	printf("%s\n", message);
	    	BroadcastToServers(message);
	    	if(numServers == 1){
	    		printf("Trying to set a monitor that is not currently assigned.%d\n", monitorNum);
				strcpy(message, "InvalidMonitor");
				SendMessage(message);
	    	}
		}
		else {
	      printf("No empty requests in requestTable\n");
	      strcpy(message, "TableFull");
	      SendMessage(message);
	    }
	}
}

void ServerDestroyMonitor(char* data){
	int monitorNum;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> monitorNum;
	if(monitorNum < 0 || monitorNum >= numMonitors){
		printf("invalid monitor index %d\n", monitorNum);
		strcpy(message, "InvalidMonitor");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	} 
	if(!serverMonitorMap.Test(monitorNum)){
		printf("monitor not currently assigned %d\n", monitorNum);
		strcpy(message, "InvalidMonitor");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}
	serverMonitorMap.Clear(monitorNum);
	servermonitorTable[monitorNum].value = -1;
	strcpy(message, "DestroyedMonitor");
	outPktHdr.to = inPktHdr.from;
	outMailHdr.to = inMailHdr.from;
 	outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);

}

void SendClientMessage(char message[MaxMailSize], int data, int index){
	char num[MaxMailSize];
	int mID, mBox;
	index += machineId*1000;
	std::stringstream ls;
	ls << index;
	ls >> num;
	strcat(message, num);
	mID = data/1000;
	mBox = data % 100;
	SendMessage(message, mID, mBox);
}

void SendResponseMessage(char response[MaxMailSize], int index){
	char prIndex[MaxMailSize], message[MaxMailSize];
	std::stringstream ss;
	ss << index;
	ss >> prIndex;
	strcpy(message, "Reply ");
	strcat(message, response);
	strcat(message, prIndex);
	SendMessage(message);
}

void ServerHandleServerRequest(char* request){
	char call[MaxMailSize];
	int data, index;
	std::stringstream ss;
	ss << request;
	ss >> call;
	ss >> call;
	if(!strcmp(call, "CL")){
		char name[MaxMailSize], message[MaxMailSize];
		ss >> name;
		ss >> data;
		ss >> index;
		printf("Call [%s], Name [%s]\n", call, name);
		int i = CheckLockTable(name);
		if(i != -1){
    	    strcpy(message, "LockNum ");	        	
	    	SendClientMessage(message, data, i);	        	
   		 	SendResponseMessage("yes ", index);
			return;
		}
	    SendResponseMessage("no ", index);
	}
	else if(!strcmp(call, "AL")){
		 int lockNum;
		 ss >> lockNum;
		 ss >> data;
		 ss >> index;
		 if(lockNum/1000 == machineId){
		 	AcquireLock(lockNum%1000, data/1000, data%100);
		 	SendResponseMessage("yes ", index);
		 	return;
		 }
		 else{
		 	SendResponseMessage("no ", index);
		 }
	}
	else if(!strcmp(call, "RL")){
		 int lockNum;
		 ss >> lockNum;
		 ss >> data;
		 ss >> index;
		 if(lockNum/1000 == machineId){
		 	ReleaseLock(lockNum%1000, data/1000, data%100);
		 	SendResponseMessage("yes ", index);
		 	return;
		 }
		 else{
		 	SendResponseMessage("no ", index);
		 }
	}
	else if(!strcmp(call, "CC")){
		char name[MaxMailSize], message[MaxMailSize];
		ss >> name;
		ss >> data;
		ss >> index;
		printf("Call [%s], Name [%s]\n", call, name);
		int i = CheckConditionTable(name);
		if(i != -1){
    	    strcpy(message, "ConditionNum ");	        	
	    	SendClientMessage(message, data, i);	        	
   		 	SendResponseMessage("yes ", index);
			return;
		}
	    SendResponseMessage("no ", index);
	}
	else if(!strcmp(call, "CM")){
		char name[MaxMailSize], message[MaxMailSize];
		ss >> name;
		ss >> data;
		ss >> index;
		printf("Call [%s], Name [%s]\n", call, name);
		int i = CheckMonitorTable(name);
		if(i != -1){
    	    strcpy(message, "MonitorNum ");	        	
	    	SendClientMessage(message, data, i);	        	
   		 	SendResponseMessage("yes ", index);
			return;
		}
	    SendResponseMessage("no ", index);
	}
	else if(!strcmp(call, "VL")){
		int lockNum;
		ss >> lockNum;
		ss >> lockNum;
		ss >> data;
		ss >> index;
		if(lockNum/1000 == machineId){
			if(VerifyLock(lockNum%1000, data/1000, data%100)){
				SendResponseMessage("yes ", index);
				return;
			}
		}
		SendResponseMessage("no ", index);
	}
	else if(!strcmp(call, "SC")){
		int conditionNum, lockNum;
		ss >> conditionNum;
		ss >> lockNum;
		ss >> data;
		ss >> index;
		if(conditionNum/1000 == machineId){
			SignalCondition(conditionNum%1000, lockNum, data/1000, data%100);
			SendResponseMessage("yes ", index);
			return;
		}
		SendResponseMessage("no ", index);
	}
	else if(!strcmp(call, "WC")){
		int conditionNum, lockNum;
		ss >> conditionNum;
		ss >> lockNum;
		ss >> data;
		ss >> index;
		if(conditionNum/1000 == machineId){
			WaitCondition(conditionNum%1000, lockNum, data/1000, data%100);
			SendResponseMessage("yes ", index);
			return;
		}
		SendResponseMessage("no ", index);
	}
	else if(!strcmp(call, "BC")){
		int conditionNum, lockNum;
		ss >> conditionNum;
		ss >> lockNum;
		ss >> data;
		ss >> index;
		if(conditionNum/1000 == machineId){
			BroadcastCondition(conditionNum%1000, lockNum, data/1000, data%100);
			SendResponseMessage("yes ", index);
			return;
		}
		SendResponseMessage("no ", index);
	}
	else if(!strcmp(call, "RWC")){
		int lockNum;
		ss >> lockNum;
		if(lockNum/1000 == machineId){
			ReleaseWaitingClient(lockNum%1000);
		}
	}
	else if(!strcmp(call, "SM")){
		int monitorNum, value;
		ss >> monitorNum;
		ss >> value;
		ss >> data;
		ss >> index;
		if(monitorNum/1000 == machineId){
			SetMonitor(monitorNum%1000, value, data/1000, data%100);
			SendResponseMessage("yes ", index);
			return;
		}
		SendResponseMessage("no ", index);
	}
	else if(!strcmp(call, "GM")){
		int monitorNum;
		ss >> monitorNum;
		ss >> data;
		ss >> index;
		if(monitorNum/1000 == machineId){
			GetMonitor(monitorNum%1000, data/1000, data%100);
			SendResponseMessage("yes ", index);
			return;
		}
		SendResponseMessage("no ", index);
	}
}

void HandleReply(char response[MaxMailSize], int index){
	if(pendingrequestMap.Test(index)){
		if(!strcmp(response, "yes")){
			if(!strcmp(pendingrequestTable[index].requestType, "VL")){
				char message[MaxMailSize];
				int index1 = pendingrequestMap.Find();
		    	if(index1 != -1){
		    		strcpy(pendingrequestTable[index1].requestType, pendingrequestTable[index].call);
		    		pendingrequestTable[index1].numNoResponses = 0;
		    		pendingrequestTable[index1].mailbox = pendingrequestTable[index].mailbox;
		    		pendingrequestTable[index1].machineId = pendingrequestTable[index].machineId;
					strcpy(message, "Server ");
					strcat(message, pendingrequestTable[index].call);
					strcat(message, " ");
					strcat(message, pendingrequestTable[index].name);
					AppendReplyData(message, index1, pendingrequestTable[index].machineId, pendingrequestTable[index].mailbox);
			    	printf("%s\n", message);
			    	std::stringstream ss;
			    	int lockNum, conditionNum;
			    	ss << pendingrequestTable[index].name;
			    	ss >> conditionNum;
			    	ss >> lockNum;
			    	if(conditionNum/1000 == machineId){
			    		if(!strcmp(pendingrequestTable[index].call, "SC")){
			    			SignalCondition(conditionNum%1000, lockNum, pendingrequestTable[index].machineId, pendingrequestTable[index].mailbox);
			    		}
			    		else if(!strcmp(pendingrequestTable[index].call, "WC")){
			    			WaitCondition(conditionNum%1000, lockNum, pendingrequestTable[index].machineId, pendingrequestTable[index].mailbox);
			    		}
			    		else if(!strcmp(pendingrequestTable[index].call, "BC")){
			    			BroadcastCondition(conditionNum%1000, lockNum, pendingrequestTable[index].machineId, pendingrequestTable[index].mailbox);
			    		}
			    	}
			    	else{
			    		BroadcastToServers(message);
			    	}
			    	if(numServers == 1){
			    		//printf("Trying to release a lock that is not currently assigned.%d\n", lockNum);
						strcpy(message, "InvalidCondition");
						SendMessage(message, pendingrequestTable[index].machineId, pendingrequestTable[index].mailbox);
			    	}
				}
				else {
			      printf("No empty requests in requestTable\n");
			      strcpy(message, "TableFull");
			      SendMessage(message, pendingrequestTable[index].machineId, pendingrequestTable[index].mailbox);
			    }
			}
			pendingrequestMap.Clear(index);
			pendingrequestTable[index].numNoResponses = 0;
		}
		else if(!strcmp(response, "no")){
			pendingrequestTable[index].numNoResponses++;
			if(pendingrequestTable[index].numNoResponses == numServers-1){
				if(!strcmp(pendingrequestTable[index].requestType, "CL")){
					CreateLock(pendingrequestTable[index].name, pendingrequestTable[index].machineId, pendingrequestTable[index].mailbox);
				}
				else if(!strcmp(pendingrequestTable[index].requestType, "AL") || !strcmp(pendingrequestTable[index].requestType, "RL") || !strcmp(pendingrequestTable[index].requestType, "VL")){
					char message[MaxMailSize];
					strcpy(message, "InvalidLock");
					SendMessage(message, pendingrequestTable[index].machineId, pendingrequestTable[index].mailbox);
				}
				else if(!strcmp(pendingrequestTable[index].requestType, "CC")){
					CreateCondition(pendingrequestTable[index].name, pendingrequestTable[index].machineId, pendingrequestTable[index].mailbox);
				}
				else if(!strcmp(pendingrequestTable[index].requestType, "CM")){
					CreateMonitor(pendingrequestTable[index].name, pendingrequestTable[index].machineId, pendingrequestTable[index].mailbox);
				}
				else if(!strcmp(pendingrequestTable[index].requestType, "SC") || !strcmp(pendingrequestTable[index].requestType, "WC") || !strcmp(pendingrequestTable[index].requestType, "BC")){
					char message[MaxMailSize];
					strcpy(message, "InvalidCondition");
					SendMessage(message, pendingrequestTable[index].machineId, pendingrequestTable[index].mailbox);
				}
				else if(!strcmp(pendingrequestTable[index].requestType, "SM") || !strcmp(pendingrequestTable[index].requestType, "GM")){
					char message[MaxMailSize];
					strcpy(message, "InvalidMonitorr");
					SendMessage(message, pendingrequestTable[index].machineId, pendingrequestTable[index].mailbox);
				}
				pendingrequestMap.Clear(index);
				pendingrequestTable[index].numNoResponses = 0;
			}
		}
	}
}

void ProcessMessage(char* message) {
	char call[MaxMailSize];
	std::stringstream ss;
	ss << message;
	ss >> call;
	printf("Call %s\n", call);
	if(!strcmp(call, "CL")){
		char name[MaxMailSize];
		ss >> name;
		printf("Call [%s], Name [%s]\n", call, name);
		ServerCreateLock(name);
	}
	else if(!strcmp(call, "AL")){
		char data[MaxMailSize];
		ss >> data;
		printf("Call [%s], Data [%s]\n", call, data);
		ServerAcquireLock(data);
	}
	else if(!strcmp(call, "RL")){
		char data[MaxMailSize];
		ss >> data;
		printf("Call [%s], Data [%s]\n", call, data);
		ServerReleaseLock(data);
	}
	else if(!strcmp(call, "DL")){
		char data[MaxMailSize];
		ss >> data;
		printf("Call [%s], Data [%s]\n", call, data);
		ServerDestroyLock(data);
	}
	else if(!strcmp(call, "CC")){
		char name[MaxMailSize];
		ss >> name;
		printf("Call [%s], Name [%s]\n", call, name);
		ServerCreateCondition(name);
	} 	
	else if(!strcmp(call, "WC")){
		char data[MaxMailSize], lockIndex[MaxMailSize];
		ss >> data;
		ss >> lockIndex;
		strcat(data, " ");
		strcat(data, lockIndex);
		printf("Call [%s], Data [%s]\n", call, data);
		ServerWaitCondition(data);
	}
	else if(!strcmp(call, "SC")){
		char data[MaxMailSize], lockIndex[MaxMailSize];
		ss >> data;
		ss >> lockIndex;
		strcat(data, " ");
		strcat(data, lockIndex);
		printf("Call [%s], Data [%s]\n", call, data);
		ServerSignalCondition(data);
	}
	else if(!strcmp(call, "BC")){
		char data[MaxMailSize], lockIndex[MaxMailSize];
		ss >> data;
		ss >> lockIndex;
		strcat(data, " ");
		strcat(data, lockIndex);
		printf("Call [%s], Data [%s]\n", call, data);
		ServerBroadcastCondition(data);
	}
	else if(!strcmp(call, "DC")){
		char data[MaxMailSize];
		ss >> data;
		printf("Call [%s], Data [%s]\n", call, data);
		ServerDestroyCondition(data);
	}
	else if(!strcmp(call, "CM")){
		char name[MaxMailSize];
		ss >> name;
		printf("Call [%s], Name [%s]\n", call, name);
		ServerCreateMonitor(name);
	}
	else if(!strcmp(call, "SM")){
		char data[MaxMailSize], value[MaxMailSize];
		ss >> data;
		ss >> value;
		strcat(data, " ");
		strcat(data, value);
		printf("Call [%s], Data [%s]\n", call, data);
		ServerSetMonitor(data);
	}
	else if(!strcmp(call, "GM")){
		char data[MaxMailSize];
		ss >> data;
		printf("Call [%s], Data [%s]\n", call, data);
		ServerGetMonitor(data);
	}
	else if(!strcmp(call, "DM")){
		char data[MaxMailSize];
		ss >> data;
		printf("Call [%s], Data [%s]\n", call, data);
		ServerDestroyMonitor(data);
	}
	else if(!strcmp(call, "Server")){
		char request[MaxMailSize], index[MaxMailSize];
		ss >> request;
		ss >> index;
		strcat(request, " ");
		strcat(request, index);
		printf("Call [%s], Data [%s]\n", call, request);
		ServerHandleServerRequest(message);
	}
	else if(!strcmp(call, "Reply")){
		char response[MaxMailSize];
		int index;
		ss >> response;
		ss >> index;
		HandleReply(response, index);
	}
}

void Server(){
    outMailHdr.from = 0;
    serverlockTable = new ServerLock[numLocks];
    serverconditionTable = new ServerCondition[numLocks];
    servermonitorTable = new ServerMonitor[numMonitors];
    pendingrequestTable = new PendingRequest[numRequests];
    char buffer[MaxMailSize];
    while(true) {
    	printf("Going to wait for message\n");
    	postOffice->Receive(0, &inPktHdr, &inMailHdr, buffer);
    	printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
    	ProcessMessage(buffer);
    	fflush(stdout);
    }
}
