#include "syscall.h"
#include "passport.h"


bool appAccepted; 
bool isPicTaken;
bool appFiled;
bool hasPassport;
bool hasPaidCashier;
int cash;
int ssn;


void initialize(){
	appAccepted = false;
	isPicTaken = false;
	appFiled = false;
	hasPassport = false;
	hasPaidCashier = false;
	cash = (Rand()%4)*500 + 100;
	AcquireRPC(numCustomersLock);
	ssn = GetMonitorRPC(customerIndex);
	if(ssn == -1)
		ssn = 0;
	SetMonitorRPC(customerIndex, ssn + 1);

	if(ssn >= MAX_CUSTOMERS){
		PrintfInt("Customer %d created\n", sizeof("Customer %d created\n"), ssn);
	}else{
		PrintfInt("Customer %d created\n", sizeof("Customer %d created\n"), ssn);	
	}
	ReleaseRPC(numCustomersLock);


}


bool gotoAppClerk(){
	bool bribe;
	int line;
	AcquireRPC(appLineLock);
	line = chooseLine(appClerks, numAppClerks, &bribe);
	if(line == -1){
		ReleaseRPC(appLineLock);
		Yield();
		return false;
	}
	else if(!bribe) { /*Get in regular line*/
		changeMonitor(appClerks[line].numLine, 1);

		PrintfInt("Customer %d has gotten in regular line for ApplicationClerk %d\n", sizeof("Customer %d has gotten in regular line for ApplicationClerk %d\n"), 1000+1000*ssn + line);
		WaitRPC(appClerks[line].waitCondition, appLineLock);

		changeMonitor(appClerks[line].numLine, -1);

	}
	else if(bribe) { /*Get in bribe line*/

		changeMonitor(appClerks[line].numBribeLine, 1);


		PrintfInt("Customer %d has gotten in bribe line for ApplicationClerk %d\n", sizeof("Customer %d has gotten in bribe line for ApplicationClerk %d\n"), 1000+1000* ssn +  line);
		PrintfInt("ApplicationClerk %d has received $500 from Customer %d\n", sizeof("ApplicationClerk %d has received $500 from Customer %d\n"), 1000+1000*line + ssn);
		cash -= 500;

		changeMonitor(appClerkCash, 500);


		WaitRPC(appClerks[line].bribeWaitCondition, appLineLock);

		changeMonitor(appClerks[line].numBribeLine, -1);

	}
	else {
			ReleaseRPC(appLineLock);

		return false;
	}
	PrintfInt("Customer %d is now at ApplicationClerk %d\n", sizeof("Customer %d is now at ApplicationClerk %d\n"), 1000+1000*ssn + line);

	ReleaseRPC(appLineLock);
	AcquireRPC(appClerks[line].ClerkLock);
	SetMonitorRPC(appClerks[line].dataIn, ssn);
	PrintfInt("Customer %d has given SSN %d to ApplicationClerk %d\n", sizeof("Customer %d has given SSN %d to ApplicationClerk %d\n"), 100000+100000*ssn + 1000+1000*ssn + line);
	SignalRPC(appClerks[line].ClerkCV, appClerks[line].ClerkLock);
	WaitRPC(appClerks[line].ClerkCV, appClerks[line].ClerkLock);
	appAccepted = true;
	SignalRPC(appClerks[line].ClerkCV, appClerks[line].ClerkLock);/*Signal that customer is leaving counter*/
	ReleaseRPC(appClerks[line].ClerkLock);
	return true;

}

int chooseLine(Clerk* clerks, int num, bool* bribe){
	int myLine = -1;
	int lineSize = 10000;
	int i;	
	*bribe = false;
	for(i = 0; i < num; i++){

		if(GetMonitorRPC(clerks[i].numLine) < lineSize && 
			GetMonitorRPC(clerks[i].state) != onbreak){
			myLine = i;
			lineSize = GetMonitorRPC(clerks[i].numLine);
			*bribe = false;
		}else if(cash >= 500 && 
			GetMonitorRPC(clerks[i].numBribeLine) < lineSize && 
			GetMonitorRPC(clerks[i].state) != onbreak){
			myLine = i;
			lineSize = GetMonitorRPC(clerks[i].numBribeLine);
			*bribe = true;
		}
	}
	return myLine;


}



int main(){
	
	if(setup()){
		int clerkChoice;

		initialize();

		while(!appAccepted){
			clerkChoice = Rand() % 4;
			switch(clerkChoice){
				case 0:
					if(!appAccepted) {
						appAccepted = gotoAppClerk();
					}
					break;

				default:
				break;
			}
			Yield();

		}

	}else{
		Write("Failed to setup Customer", sizeof("Failed to setup Customer"), ConsoleOutput);
	}
	PrintfInt("Customer %d leaving\n", sizeof("Customer %d leaving\n"), ssn);

	Exit(1);
}