#include "syscall.h"
#include "passport.h"


void initializeCustomerData(){
	int i;
	for(i = 0; i < MAX_CUSTOMERS; i++){
		CustomerData* c = &customerDatabase[i];
		SetMonitorRPC(c->appAccepted, false);
		SetMonitorRPC(c->isPicTaken, false);
		SetMonitorRPC(c->appFiled, false);
		SetMonitorRPC(c->hasPassport, false);
		SetMonitorRPC(c->hasPaidCashier, false);
		SetMonitorRPC(c->ssn, -1);

		
	}
}

void initializeClerkData(){
	int i;
	SetMonitorRPC(appClerkCash, 0);

	for(i = 0; i < numAppClerks; i++) {
		Clerk* ac = &appClerks[i];
		SetMonitorRPC(ac->numLine, 0);
		SetMonitorRPC(ac->numBribeLine, 0);
		SetMonitorRPC(ac->state, available);

	}
}

int main(){
	if(setup()){
		initializeCustomerData();
		initializeClerkData();
	}
}