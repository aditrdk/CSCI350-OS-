#include "syscall.h"

typedef enum {available, busy, onbreak} CLERK_STATE;	
typedef enum { false, true } bool;


/*Customer data used by the Passport Office database that the clerks can look up via ssn*/
typedef struct {
	bool appAccepted;
	bool isPicTaken;
	bool appFiled;
	bool hasPassport;
	bool hasPaidCashier;
	int ssn;
} CustomerData;

/*Generic clerk data*/
typedef struct {
	int numLine;	
	int numBribeLine;
	CLERK_STATE state;		/*keep track of clerk state*/
	int waitCondition;	/*clerk uses this condition to signal next customer in line*/
	int bribeWaitCondition;	/*clerk uses this condition to signal next customer in line*/
	int ClerkLock; 
	int ClerkCV;
	int dataIn;				/*Variable for passing data between clerk and customer*/
}  Clerk;


/*Variable set based on the test being run*/
int numCustomers;
int numSenators;
int numAppClerks;
int numPicClerks;
int numPassClerks;
int numCashClerks;

/*Test number*/
int userChoice;

CustomerData customerDatabase[50];

/*Data that can be changed by any thread. Each has their own associated lock to ensure mutal exclusion*/
int numCustomersWhoHaveLeftTheOffice = 0;
int numSenatorsWhoHaveLeftTheOffice = 0;

int numCustomersLock;
int customerIndex;

int SenatorLock;
int SenatorWaitCondition;
int numSenatorsPresent = 0; 
int numCustomersOutside = 0;
int senatorIndex = 0;

int appLineLock;
int appBreakCondition;
Clerk appClerks[5];
int numAppClerksAwake;
int appClerkCash; /*Bribe line money Locked by line lock*/
int appIndex = 0;

int picLineLock;
int picBreakCondition;
Clerk picClerks[5];
int numPicClerksAwake;
int picClerkCash = 0; /*Bribe line money Locked by line lock*/
int picIndex = 0;

int passLineLock;
int passBreakCondition;
Clerk passClerks[5];
int numPassClerksAwake;
int passClerkCash= 0; /*Bribe line money Locked by line lock*/
int passIndex = 0;

int cashLineLock;
int cashBreakCondition;
Clerk cashClerks[5];
int numCashClerksAwake;
int cashClerkCash= 0; /*Locked by line lock and special cashCashLock*/
int cashCashLock; /*extra lock for cashier cash variable because it is modified other than during the line bribing*/
int cashIndex = 0;

#define MAX_LENGTH 25
#define MAX_CUSTOMERS 30

void changeMonitor(int monitor, int amt){
	int x = GetMonitorRPC(monitor);
	SetMonitorRPC(monitor, x + amt);

}


int setup(){
	int i;
	numAppClerks = 3;

	/* Create Locks Conditions and Monitors */
	numCustomersLock = CreateLockRPC("NumCustomersLock", sizeof("NumCustomersLock"));
	customerIndex = CreateMonitorRPC("CustomerIndex", sizeof("CustomerIndex"));

	SenatorLock = CreateLockRPC("SenatorLock", sizeof("SenatorLock"));
	SenatorWaitCondition = CreateConditionRPC("SenatorWaitCV", sizeof("SenatorWaitCV"));

	appLineLock = CreateLockRPC("AppLineLock", sizeof("AppLineLock"));
	appBreakCondition = CreateConditionRPC("AppBreakCV", sizeof("AppBreakCV" ));
	appIndex = CreateMonitorRPC("AppIndex", sizeof("AppIndex"));
	appClerkCash = CreateMonitorRPC("AppClerkCash", sizeof("AppClerkCash"));

	for(i = 0; i < MAX_CUSTOMERS; i++){
		char name[MAX_LENGTH];

		CustomerData* c = &customerDatabase[i];
		if(i < 10){
			SPrintfInt(name, "appAcceptedCust%d", sizeof("appAcceptedCust%d"), i);
			c->appAccepted = CreateMonitorRPC(name, sizeof("appAcceptedCust1"));

			SPrintfInt(name, "isPicTakenCust%d", sizeof("isPicTakenCust%d"), i);
			c->isPicTaken = CreateMonitorRPC(name, sizeof("isPicTakenCust1"));

			SPrintfInt(name, "appFiledCust%d", sizeof("appFiledCust%d"), i);
			c->appFiled = CreateMonitorRPC(name, sizeof("appFiledCust1"));

			SPrintfInt(name, "hasPassportCust%d", sizeof("hasPassportCust%d"), i);
			c->hasPassport = CreateMonitorRPC(name, sizeof("hasPassportCust1"));

			SPrintfInt(name, "hasPaidCashierCust%d", sizeof("hasPaidCashierCust%d"), i);
			c->hasPaidCashier = CreateMonitorRPC(name, sizeof("hasPaidCashierCust1"));

			SPrintfInt(name, "ssnCust%d", sizeof("ssnCust%d"), i);
			c->ssn = CreateMonitorRPC(name, sizeof("ssnCust1"));
		}
		else{

			SPrintfInt(name, "appAcceptedCust%d", sizeof("appAcceptedCust%d"), i);
			c->appAccepted = CreateMonitorRPC(name, sizeof("appAcceptedCust%d"));

			SPrintfInt(name, "isPicTakenCust%d", sizeof("isPicTakenCust%d"), i);
			c->isPicTaken = CreateMonitorRPC(name, sizeof("isPicTakenCust%d"));

			SPrintfInt(name, "appFiledCust%d", sizeof("appFiledCust%d"), i);
			c->appFiled = CreateMonitorRPC(name, sizeof("appFiledCust%d"));

			SPrintfInt(name, "hasPassportCust%d", sizeof("hasPassportCust%d"), i);
			c->hasPassport = CreateMonitorRPC(name, sizeof("hasPassportCust%d"));

			SPrintfInt(name, "hasPaidCashierCust%d", sizeof("hasPaidCashierCust%d"), i);
			c->hasPaidCashier = CreateMonitorRPC(name, sizeof("hasPaidCashierCust%d"));

			SPrintfInt(name, "ssnCust%d", sizeof("ssnCust%d"), i);
			c->ssn = CreateMonitorRPC(name, sizeof("ssnCust%d"));

		}
	}

	
	/*
	picLineLock = CreateLockRPC("PicLineLock", sizeof("PicLineLock"));
	picBreakCondition = CreateConditionRPC("PicBreakCV", sizeof("PicBreakCV"));
	
	passLineLock = CreateLockRPC("PassLineLock", sizeof("PassLineLock"));
	passBreakCondition = CreateConditionRPC("PassBreakCV", sizeof("PassBreakCV"));
	
	cashLineLock = CreateLockRPC("CashLineLock", sizeof("CashLineLock"));
	cashBreakCondition = CreateConditionRPC("CashBreakCV", sizeof("CashBreakCV"));
	cashCashLock = CreateLockRPC("CashCashLock", sizeof("CashCashLock")); 
	*/

	for(i = 0; i < numAppClerks; i++) {
		char name[MAX_LENGTH];
		Clerk* ac = &appClerks[i];
		SPrintfInt(name, "appClerk_NumLine%d", sizeof("appClerk_NumLine%d"), i);
		ac->numLine = CreateMonitorRPC(name, sizeof("appClerk_NumLine1"));
		if(GetMonitorRPC(ac->numLine) == -1){
			SetMonitorRPC(ac->numLine, 0);
		}

		SPrintfInt(name, "appClerk_NumBribeLine%d", sizeof("appClerk_NumBribeLine%d"), i);
		ac->numBribeLine = CreateMonitorRPC(name, sizeof("appClerk_NumBribeLine1"));
		if(GetMonitorRPC(ac->numBribeLine) == -1){
			SetMonitorRPC(ac->numBribeLine, 0);
		}

		SPrintfInt(name, "appClerk_State%d", sizeof("appClerk_State%d"), i);
		ac->state = CreateMonitorRPC(name, sizeof("appClerk_State1"));
		if(GetMonitorRPC(ac->state) == -1){
			SetMonitorRPC(ac->state, available);
		}

		SPrintfInt(name, "appClerk_WaitCV%d", sizeof("appClerk_WaitCV%d"), i);
		ac->waitCondition = CreateConditionRPC(name, sizeof("appClerk_WaitCV1"));

		SPrintfInt(name, "appClerk_BribeWaitCV%d", sizeof("appClerk_BribeWaitCV%d"), i);
		ac->bribeWaitCondition = CreateConditionRPC(name, sizeof("appClerk_BribeWaitCV1"));

		SPrintfInt(name, "appClerk_ClerkLock%d", sizeof("appClerk_ClerkLock%d"), i);
		ac->ClerkLock = CreateLockRPC(name, sizeof("appClerk_ClerkLock1"));

		SPrintfInt(name, "appClerk_ClerkCV%d", sizeof("appClerk_ClerkCV%d"), i);
		ac->ClerkCV = CreateConditionRPC(name, sizeof("appClerk_ClerkCV1"));

		SPrintfInt(name, "appClerk_DataIn%d", sizeof("appClerk_DataIn%d"), i);
		ac->dataIn = CreateMonitorRPC(name, sizeof("appClerk_DataIn1"));
	}
	/*
	for(i = 0; i < numPicClerks; i++) {
		Clerk* pc = &picClerks[i];
		pc->numLine = 0;
		pc->numBribeLine = 0;
		pc->servedOne = false;
		pc->state = busy;
		pc->waitCondition = CreateCondition("picClerk_WaitCondition", sizeof("picClerk_WaitCondition"));
		pc->bribeWaitCondition = CreateCondition("picClerk_BribeWaitCondition", sizeof("picClerk_BribeWaitCondition"));
		pc->ClerkLock = CreateLock("picClerk_ClerkLock", sizeof("picClerk_ClerkLock"));
		pc->ClerkCV = CreateCondition("picClerk_ClerkCV", sizeof("picClerk_ClerkCV"));
	}
	for(i = 0; i < numPassClerks; i++) {
		Clerk* ac = &passClerks[i];
		ac->numLine = 0;
		ac->numBribeLine = 0;
		ac->servedOne = false;
		ac->state = busy;
		ac->waitCondition = CreateCondition("passClerk_WaitCondition", sizeof("passClerk_WaitCondition"));
		ac->bribeWaitCondition = CreateCondition("passClerk_BribeWaitCondition", sizeof("passClerk_BribeWaitCondition"));
		ac->ClerkLock = CreateLock("passClerk_ClerkLock", sizeof("passClerk_ClerkLock"));
		ac->ClerkCV = CreateCondition("passClerk_ClerkCV", sizeof("passClerk_ClerkCV"));
	}
	for(i = 0; i < numCashClerks; i++) {
		Clerk* ac = &cashClerks[i];
		ac->numLine = 0;
		ac->numBribeLine = 0;
		ac->servedOne = false;
		ac->state = busy;
		ac->waitCondition = CreateCondition("cashClerk_WaitCondition", sizeof("cashClerk_WaitCondition"));
		ac->bribeWaitCondition = CreateCondition("cashClerk_BribeWaitCondition", sizeof("cashClerk_BribeWaitCondition"));
		ac->ClerkLock = CreateLock("cashClerk_ClerkLock", sizeof("cashClerk_ClerkLock"));
		ac->ClerkCV = CreateCondition("cashClerk_ClerkCV", sizeof("cashClerk_ClerkCV"));
	}
	*/
	return 1;
}