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

ServerLock* serverlockTable;
BitMap serverLockMap(numLocks);

ServerCondition* serverconditionTable;
BitMap serverConditionMap(numConditions);

int numMonitors = numLocks;
ServerMonitor* servermonitorTable;
BitMap serverMonitorMap(numMonitors);

void ErrorMessage(char emessage[]){
	char message[MaxMailSize];
	strcpy(message, "Error ");
	strcat(message, emessage);
	outPktHdr.to = inPktHdr.from;
   	outMailHdr.to = inMailHdr.from;
   	outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);
}

void ServerCreateLock(char name[]){
	char message[MaxMailSize];
	std::stringstream ss;
	strcpy(message, "LockNum ");
	char lockNum[MaxMailSize];
    for(int i = 0; i < numLocks; i++){
      if(serverLockMap.Test(i)){
        if(!strcmp(serverlockTable[i].name, name)){
        	int index = i;
        	ss << index;
        	ss >> lockNum;
        	strcat(message, lockNum);
          	outPktHdr.to = inPktHdr.from;
    		outMailHdr.to = inMailHdr.from;
   	 		outMailHdr.length = sizeof(message);
			postOffice->Send(outPktHdr, outMailHdr, message);
			return;
        }
      }
    }
    int index = serverLockMap.Find();
    if(index != -1) {
      printf("Creating New lock\n");
      ss << index;
      ss >> lockNum;
      strcat(message, lockNum);
      printf("SS'd and cat'd\n");
      strcpy(serverlockTable[index].name, name);
      serverlockTable[index].state = AVAIL;
      serverlockTable[index].isToBeDeleted = false;
      serverlockTable[index].queue = new List;
      printf("Initialized serverlock\n");
    }
    else {
      printf("No empty locks in lockTable\n");
      strcpy(message, "TableFull");
    }
	outPktHdr.to = inPktHdr.from;
    outMailHdr.to = inMailHdr.from;
    outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);
}

void ServerAcquireLock(char* data){
	int lockNum;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> lockNum;
	if(lockNum < 0 || lockNum >= numLocks){
		printf("Trying to acquire invalid lock index %d\n", lockNum);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	} 
	if(!serverLockMap.Test(lockNum)){
		printf("Trying to acquire a lock that is not currently assigned.%d\n", lockNum);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	} 
	if(serverlockTable[lockNum].isToBeDeleted){
		printf("Cannot acquire lock. Lock is marked to be deleted.%d\n", lockNum);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	} 
	if(serverlockTable[lockNum].state == AVAIL){
		serverlockTable[lockNum].state = BUSY;
		serverlockTable[lockNum].machineID = inPktHdr.from;
		serverlockTable[lockNum].port = inMailHdr.from;
		strcpy(message, "AcquiredLock ");
		std::stringstream ls;
		ls << lockNum;
		char index[MaxMailSize];
		ls >> index;
		strcat(message, index);
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
	}
	else if(serverlockTable[lockNum].machineID == inPktHdr.from && serverlockTable[lockNum].port == inMailHdr.from){
		strcpy(message, "AcquiredLock ");
		std::stringstream ls;
		ls << lockNum;
		char index[MaxMailSize];
		ls >> index;
		strcat(message, index);
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
	}
	else {
		int returnData;
		returnData = inPktHdr.from * 1000;
		returnData += inMailHdr.from;
		printf("Appending to queue\n");
		serverlockTable[lockNum].queue->Append((void*)returnData);
	}
}

void ServerReleaseLock(char* data){
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
	if(serverlockTable[lockNum].state == AVAIL){
		printf("Trying to release a lock that has not been acquired.%d\n", lockNum);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}
	if(!(serverlockTable[lockNum].machineID == inPktHdr.from && serverlockTable[lockNum].port == inMailHdr.from)){
		printf("Cannot release lock. Current thread is not holding the lock %d\n", lockNum);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}
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
		outPktHdr.to = machineID;
    	outMailHdr.to = port;
		outMailHdr.length = sizeof(message);
		printf("Going to send message [%s] to machineID %d, port %d\n", message, machineID, port);
		serverlockTable[lockNum].state = BUSY;
		serverlockTable[lockNum].machineID = machineID;
		serverlockTable[lockNum].port = port;
		postOffice->Send(outPktHdr, outMailHdr, message);
	}
	strcpy(message, "ReleasedLock");
	outPktHdr.to = inPktHdr.from;
    outMailHdr.to = inMailHdr.from;
   	outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);
	if(serverlockTable[lockNum].isToBeDeleted && serverlockTable[lockNum].queue->IsEmpty()){
		printf("Destroying lock %d\n", lockNum);
		delete serverlockTable[lockNum].queue;
		serverlockTable[lockNum].isToBeDeleted = false;
		serverLockMap.Clear(lockNum);
	}
}

void ServerSignalCondition(char* data){
	int lockNum, conditionNum;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> conditionNum;
	ss >> lockNum;
	if(lockNum < 0 || lockNum >= numLocks){
		printf("invalid lock index %d\n", lockNum);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}
	if(conditionNum < 0 || conditionNum >= numConditions){
		printf("invalid condition index %d\n", conditionNum);
		strcpy(message, "InvalidCondition");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	} 
	if(!serverLockMap.Test(lockNum)){
		printf("lock that not currently assigned.%d\n", lockNum);
		strcpy(message, "InvalidLock");
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
	if(!(serverlockTable[lockNum].machineID == inPktHdr.from && serverlockTable[lockNum].port == inMailHdr.from)){
		printf("Cannot wait on condition. Current thread is not holding the lock %d\n", lockNum);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}
	if(serverconditionTable[conditionNum].lockIndex != lockNum){
		printf("Condition Lock %d does not match Waiting Lock %d.\n", lockNum, serverconditionTable[conditionNum].lockIndex);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}
	if(serverconditionTable[conditionNum].queue->IsEmpty()){
		strcpy(message, "SignalledCondition");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
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
	outPktHdr.to = machineID;
	outMailHdr.to = port;
	outMailHdr.length = sizeof(message);
	printf("Going to send message to machineID %d, port %d\n", machineID, port);
	postOffice->Send(outPktHdr, outMailHdr, message);	
	if(serverconditionTable[conditionNum].queue->IsEmpty()){
		serverconditionTable[conditionNum].lockIndex = -1;
	}
	strcpy(message, "SignalledCondition");
	outPktHdr.to = inPktHdr.from;
	outMailHdr.to = inMailHdr.from;
	outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);
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
	if(lockNum < 0 || lockNum >= numLocks){
		printf("invalid lock index %d\n", lockNum);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}
	if(conditionNum < 0 || conditionNum >= numConditions){
		printf("invalid condition index %d\n", conditionNum);
		strcpy(message, "InvalidCondition");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	} 
	if(!serverLockMap.Test(lockNum)){
		printf("lock that not currently assigned.%d\n", lockNum);
		strcpy(message, "InvalidLock");
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
	if(!(serverlockTable[lockNum].machineID == inPktHdr.from && serverlockTable[lockNum].port == inMailHdr.from)){
		printf("Cannot wait on condition. Current thread is not holding the lock %d\n", lockNum);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}
	if(serverconditionTable[conditionNum].lockIndex != lockNum){
		printf("Condition Lock %d does not match Waiting Lock %d.\n", lockNum, serverconditionTable[conditionNum].lockIndex);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}
	while(!serverconditionTable[conditionNum].queue->IsEmpty()){
		ServerSignalCondition(data);
	}
	strcpy(message, "BroadcastedCondition");
	outPktHdr.to = inPktHdr.from;
	outMailHdr.to = inMailHdr.from;
	outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);
	if(serverconditionTable[conditionNum].isToBeDeleted && serverconditionTable[conditionNum].queue->IsEmpty()){
		printf("Destroying condition %d\n", conditionNum);
		delete serverconditionTable[conditionNum].queue;
		serverconditionTable[conditionNum].isToBeDeleted = false;
		serverConditionMap.Clear(conditionNum);
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

void ServerCreateCondition(char name[]){
	char message[MaxMailSize];
	std::stringstream ss;
	strcpy(message, "ConditionNum ");
	char conditionNum[MaxMailSize];
    for(int i = 0; i < numConditions; i++){
      if(serverConditionMap.Test(i)){
        if(!strcmp(serverconditionTable[i].name, name)){
        	int index = i;
        	ss << index;
        	ss >> conditionNum;
        	strcat(message, conditionNum);
          	outPktHdr.to = inPktHdr.from;
    		outMailHdr.to = inMailHdr.from;
   	 		outMailHdr.length = sizeof(message);
			postOffice->Send(outPktHdr, outMailHdr, message);
			return;
        }
      }
    }
    int index = serverConditionMap.Find();
    if(index != -1) {
      printf("Creating New condition\n");
      ss << index;
      ss >> conditionNum;
      strcat(message, conditionNum);
      printf("SS'd and cat'd\n");
      strcpy(serverconditionTable[index].name, name);
      serverconditionTable[index].isToBeDeleted = false;
      serverconditionTable[index].lockIndex = -1;
      serverconditionTable[index].queue = new List;
      printf("Initialized servercondition\n");
    }
    else {
      printf("No empty conditions in conditionTable\n");
      strcpy(message, "TableFull");
    }
	outPktHdr.to = inPktHdr.from;
    outMailHdr.to = inMailHdr.from;
    outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);
}

void ServerWaitCondition(char* data){
	int lockNum, conditionNum;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> conditionNum;
	ss >> lockNum;
	if(lockNum < 0 || lockNum >= numLocks){
		printf("invalid lock index %d\n", lockNum);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}
	if(conditionNum < 0 || conditionNum >= numConditions){
		printf("invalid condition index %d\n", conditionNum);
		strcpy(message, "InvalidCondition");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	} 
	if(!serverLockMap.Test(lockNum)){
		printf("lock that not currently assigned.%d\n", lockNum);
		strcpy(message, "InvalidLock");
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
	if(!(serverlockTable[lockNum].machineID == inPktHdr.from && serverlockTable[lockNum].port == inMailHdr.from)){
		printf("Cannot wait on condition. Current thread is not holding the lock %d\n", lockNum);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}
	if(serverconditionTable[conditionNum].isToBeDeleted){
		printf("Cannot wait on condition. Condition is marked to be deleted.%d\n", lockNum);
		strcpy(message, "InvalidCondition");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}

	if(serverconditionTable[conditionNum].lockIndex == -1){
		serverconditionTable[conditionNum].lockIndex = lockNum;
	}
	else if(serverconditionTable[conditionNum].lockIndex != lockNum){
		printf("Condition Lock %d does not match Waiting Lock %d.\n", lockNum, serverconditionTable[conditionNum].lockIndex);
		strcpy(message, "InvalidLock");
		outPktHdr.to = inPktHdr.from;
    	outMailHdr.to = inMailHdr.from;
   	 	outMailHdr.length = sizeof(message);
		postOffice->Send(outPktHdr, outMailHdr, message);
		return;
	}
	int returnData;
	returnData = inPktHdr.from * 1000;
	returnData += inMailHdr.from;
	printf("Appending to queue\n");
	serverconditionTable[conditionNum].queue->Append((void*)returnData);
	char index[MaxMailSize];
	std::stringstream bs;
	bs << lockNum;
	bs >> index;
	ServerReleaseLock(index);
}

void ServerCreateMonitor(char name[]){
	char message[MaxMailSize];
	std::stringstream ss;
	strcpy(message, "MonitorNum ");
	char monitorNum[MaxMailSize];
    for(int i = 0; i < numMonitors; i++){
      if(serverMonitorMap.Test(i)){
        if(!strcmp(servermonitorTable[i].name, name)){
        	int index = i;
        	ss << index;
        	ss >> monitorNum;
        	strcat(message, monitorNum);
          	outPktHdr.to = inPktHdr.from;
    		outMailHdr.to = inMailHdr.from;
   	 		outMailHdr.length = sizeof(message);
			postOffice->Send(outPktHdr, outMailHdr, message);
			return;
        }
      }
    }
    int index = serverMonitorMap.Find();
    if(index != -1) {
      printf("Creating New monitor\n");
      ss << index;
      ss >> monitorNum;
      strcat(message, monitorNum);
      printf("SS'd and cat'd\n");
      strcpy(servermonitorTable[index].name, name);
      servermonitorTable[index].value = -1;
      printf("Initialized servermonitor\n");
    }
    else {
      printf("No empty monitors in lockTable\n");
      strcpy(message, "TableFull");
    }
	outPktHdr.to = inPktHdr.from;
    outMailHdr.to = inMailHdr.from;
    outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);
}

void ServerSetMonitor(char * data){
	int monitorNum, value;
	std::stringstream ss;
	char message[MaxMailSize];
	ss << data;
	ss >> monitorNum;
	ss >> value;
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
	servermonitorTable[monitorNum].value = value;
	printf("Monitor %d has a value of %d",monitorNum, servermonitorTable[monitorNum].value);
	strcpy(message, "SetMonitor");
	outPktHdr.to = inPktHdr.from;
	outMailHdr.to = inMailHdr.from;
	outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);
}

void ServerGetMonitor(char* data){
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
	strcpy(message, "GotMonitor ");
	char value[MaxMailSize];
	std::stringstream bs;
	bs << servermonitorTable[monitorNum].value;
	bs >> value;
	printf("Monitor %d has a value of %s\n",monitorNum, value);
	strcat(message, value);
	outPktHdr.to = inPktHdr.from;
	outMailHdr.to = inMailHdr.from;
	outMailHdr.length = sizeof(message);
	postOffice->Send(outPktHdr, outMailHdr, message);
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

void ProcessMessage(char* message) {
	char call[MaxMailSize];
	std::stringstream ss;
	ss << message;
	ss >> call;
	printf("Call %s\n", call);
	if(!strcmp(call, "CreateLock")){
		char name[MaxMailSize];
		ss >> name;
		printf("Call [%s], Name [%s]\n", call, name);
		ServerCreateLock(name);
	}
	else if(!strcmp(call, "AcquireLock")){
		char data[MaxMailSize];
		ss >> data;
		printf("Call [%s], Data [%s]\n", call, data);
		ServerAcquireLock(data);
	}
	else if(!strcmp(call, "ReleaseLock")){
		char data[MaxMailSize];
		ss >> data;
		printf("Call [%s], Data [%s]\n", call, data);
		ServerReleaseLock(data);
	}
	else if(!strcmp(call, "DestroyLock")){
		char data[MaxMailSize];
		ss >> data;
		printf("Call [%s], Data [%s]\n", call, data);
		ServerDestroyLock(data);
	}
	else if(!strcmp(call, "CreateCondition")){
		char name[MaxMailSize];
		ss >> name;
		printf("Call [%s], Name [%s]\n", call, name);
		ServerCreateCondition(name);
	} 	
	else if(!strcmp(call, "WaitCondition")){
		char data[MaxMailSize], lockIndex[MaxMailSize];
		ss >> data;
		ss >> lockIndex;
		strcat(data, " ");
		strcat(data, lockIndex);
		printf("Call [%s], Data [%s]\n", call, data);
		ServerWaitCondition(data);
	}
	else if(!strcmp(call, "SignalCondition")){
		char data[MaxMailSize], lockIndex[MaxMailSize];
		ss >> data;
		ss >> lockIndex;
		strcat(data, " ");
		strcat(data, lockIndex);
		printf("Call [%s], Data [%s]\n", call, data);
		ServerSignalCondition(data);
	}
	else if(!strcmp(call, "BroadcastCondition")){
		char data[MaxMailSize], lockIndex[MaxMailSize];
		ss >> data;
		ss >> lockIndex;
		strcat(data, " ");
		strcat(data, lockIndex);
		printf("Call [%s], Data [%s]\n", call, data);
		ServerBroadcastCondition(data);
	}
	else if(!strcmp(call, "DestroyCondition")){
		char data[MaxMailSize];
		ss >> data;
		printf("Call [%s], Data [%s]\n", call, data);
		ServerDestroyCondition(data);
	}
	else if(!strcmp(call, "CreateMonitor")){
		char name[MaxMailSize];
		ss >> name;
		printf("Call [%s], Name [%s]\n", call, name);
		ServerCreateMonitor(name);
	}
	else if(!strcmp(call, "SetMonitor")){
		char data[MaxMailSize], value[MaxMailSize];
		ss >> data;
		ss >> value;
		strcat(data, " ");
		strcat(data, value);
		printf("Call [%s], Data [%s]\n", call, data);
		ServerSetMonitor(data);
	}
	else if(!strcmp(call, "GetMonitor")){
		char data[MaxMailSize];
		ss >> data;
		printf("Call [%s], Data [%s]\n", call, data);
		ServerGetMonitor(data);
	}
	else if(!strcmp(call, "DestroyMonitor")){
		char data[MaxMailSize];
		ss >> data;
		printf("Call [%s], Data [%s]\n", call, data);
		ServerDestroyMonitor(data);
	}
}

void Server(){
    outMailHdr.from = 0;
    serverlockTable = new ServerLock[numLocks];
    serverconditionTable = new ServerCondition[numLocks];
    servermonitorTable = new ServerMonitor[numMonitors];
    char buffer[MaxMailSize];
    while(true) {
    	printf("Going to wait for message\n");
    	postOffice->Receive(0, &inPktHdr, &inMailHdr, buffer);
    	printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
    	ProcessMessage(buffer);
    	fflush(stdout);
    }
}
