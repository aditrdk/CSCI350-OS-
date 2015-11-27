#include "syscall.h"
#include "passport.h"


int index;
bool servedOne = false;
Clerk* c;


void SignalNextCustomer(){

	if(GetMonitorRPC(c->numBribeLine) > 0){
		PrintfInt("ApplicationClerk %d has signalled a customer to come to their counter\n", sizeof("ApplicationClerk %d has signalled a customer to come to their counter\n"), index);
		SignalRPC(c->bribeWaitCondition, appLineLock);
		SetMonitorRPC(c->state, busy);		
	}else if(GetMonitorRPC(c->numLine) > 0){
		PrintfInt("ApplicationClerk %d has signalled a customer to come to their counter\n", sizeof("ApplicationClerk %d has signalled a customer to come to their counter\n"), index);
		SignalRPC(c->waitCondition, appLineLock);
		SetMonitorRPC(c->state, busy);
	}

}


int main(){
	if(setup()){
		AcquireRPC(appLineLock);
		index = GetMonitorRPC(appIndex);
		if(index == -1) index = 0;
		SetMonitorRPC(appIndex, index + 1);
		ReleaseRPC(appLineLock);
		c = &appClerks[index];

		while(true){
			AcquireRPC(appLineLock);
			if(GetMonitorRPC(c->state) == available){
				SignalNextCustomer();

			}

			if(GetMonitorRPC(c->state) == busy){
				int ssn;
				int waitTime;
				int i;

				AcquireRPC(c->ClerkLock);
				ReleaseRPC(appLineLock);

				WaitRPC(c->ClerkCV, c->ClerkLock);
				ssn = GetMonitorRPC(c->dataIn);
				PrintfInt("ApplicationClerk %d has received a SSN %d from Customer %d\n", sizeof("ApplicationClerk %d has received a SSN %d from Customer %d\n"), 100000+ 100000* index + 1000 + 1000 * ssn + ssn);
				waitTime = Rand() % 81 + 20;
				for(i = 0; i < waitTime; i++) Yield();

				SetMonitorRPC(customerDatabase[ssn].appAccepted, true);

				PrintfInt("ApplicationClerk %d has recorded a completed application for Customer %d\n", sizeof("ApplicationClerk %d has recorded a completed application for Customer %d\n"), 1000+1000* index + ssn);
				SignalRPC(c->ClerkCV, c->ClerkLock); /*Signal that application has been filed*/
				WaitRPC(c->ClerkCV, c->ClerkLock); /*Wait for customer to leave the counter*/
				servedOne = true;
				SetMonitorRPC(c->state, available);
				ReleaseRPC(c->ClerkLock);

			}else{
				ReleaseRPC(appLineLock);

			}
		}
	}else{
		Write("Failed to setup AppClerk", sizeof("Failed to setup AppClerk"), ConsoleOutput);
	}
	Exit(0);

	
}