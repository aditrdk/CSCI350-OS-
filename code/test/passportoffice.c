/*
// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
*/
#include "syscall.h"


typedef enum {available, busy, onbreak} CLERK_STATE;	
typedef enum { false, true } bool;

/*Struct for customer's private data, the variables that are not accessible by other threads
	//Customer keep track themselves of which clerks they ahve visted already successfully
*/
typedef struct {
	bool appAccepted; 
	bool isPicTaken;
	bool appFiled;
	bool hasPassport;
	bool hasPaidCashier;
	int cash;
	int ssn;

} Customer;

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
	bool servedOne;
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



/*Data that can be changed by any thread. Each has their own associated lock to ensure mutal exclusion*/
int numCustomersWhoHaveLeftTheOffice = 0;
int numSenatorsWhoHaveLeftTheOffice = 0;
int numCustomersLock;
int customerIndex = 0;

int SenatorLock;
int SenatorWaitCondition;
int numSenatorsPresent = 0; 
int numCustomersOutside = 0;
int senatorIndex = 0;

int appLineLock;
int appBreakCondition;
Clerk appClerks[5];
int numAppClerksAwake;
int appClerkCash = 0; /*Bribe line money Locked by line lock*/
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

/*Passport office's internal database. */
CustomerData customersDatabase[60];



void initializeVariables(){
	int i;
	/* Create Locks and Syscalls */
	numCustomersLock = CreateLock("NumCustomersLock", sizeof("NumCustomersLock"));

	SenatorLock = CreateLock("SenatorLock", sizeof("SenatorLock"));
	SenatorWaitCondition = CreateCondition("SenatorWaitCV", sizeof("SenatorWaitCV"));

	appLineLock = CreateLock("AppLineLock", sizeof("AppLineLock"));
	appBreakCondition = CreateCondition("AppBreakCV", sizeof("AppBreakCV" ));
	
	picLineLock = CreateLock("PicLineLock", sizeof("PicLineLock"));
	picBreakCondition = CreateCondition("PicBreakCV", sizeof("PicBreakCV"));
	
	passLineLock = CreateLock("PassLineLock", sizeof("PassLineLock"));
	passBreakCondition = CreateCondition("PassBreakCV", sizeof("PassBreakCV"));
	
	cashLineLock = CreateLock("CashLineLock", sizeof("CashLineLock"));
	cashBreakCondition = CreateCondition("CashBreakCV", sizeof("CashBreakCV"));
	cashCashLock = CreateLock("CashCashLock", sizeof("CashCashLock")); /*extra lock for cashier cash variable because it is modified other than during the line bribing*/
	for(i = 0; i < numAppClerks; i++) {
		Clerk* ac = &appClerks[i];
		ac->numLine = 0;
		ac->numBribeLine = 0;
		ac->servedOne = false;
		ac->state = busy;
		ac->waitCondition = CreateCondition("appClerk_WaitCondition", sizeof("appClerk_WaitCondition"));
		ac->bribeWaitCondition = CreateCondition("appClerk_BribeWaitCondition", sizeof("appClerk_BribeWaitCondition"));
		ac->ClerkLock = CreateLock("appClerk_ClerkLock", sizeof("appClerk_ClerkLock"));
		PrintfInt("lock is %d\n\n", sizeof("lock is %d\n\n"), ac->ClerkLock);
		ac->ClerkCV = CreateCondition("appClerk_ClerkCV", sizeof("appClerk_ClerkCV"));
	}
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

}


/*Function that customers go through at creation so they can be manipulated for better testing*/
void initializeCustomerForTest(Customer* c){
	switch(userChoice){
		case 1:
		case 3:
		case 4:
			c->appAccepted = true;
			c->isPicTaken = true;
			c->appFiled = true;
			c->cash = 100;
			customersDatabase[c->ssn].ssn = c->ssn;
			customersDatabase[c->ssn].appAccepted = true;
			customersDatabase[c->ssn].isPicTaken = true;
			customersDatabase[c->ssn].appFiled = true;
			break;
		case 2:
		case 6:
			c->cash = 600;
			break;
		case 5:					/*Causes most of the customers to enter later therefore clerks will go on break first and need to be woken up by the manager*/
			c->cash = 100;
			if(c->ssn > 6){
				int i;
				for(i = 0; i < 1000; i++) Yield();
			}
			break;
	}

}


/*Customer Main Thread*/
void CustomerThread(int index) {
	int clerkChoice;
	/*Initialize new customer*/
	Customer c;
	Acquire(numCustomersLock);
	c.ssn = customerIndex++;
	Release(numCustomersLock);
	c.appAccepted = false;
	c.isPicTaken = false;
	c.appFiled = false;
	c.hasPassport = false;
	c.hasPaidCashier = false;
	c.cash = (Rand()%4)*500 + 100;

	initializeCustomerForTest(&c);


	/*Loop until the customer has a passport*/
	while(!c.hasPassport){

		/*Check for senators*/
		Acquire(SenatorLock);
		if(numSenatorsPresent != 0){
			PrintfInt("Customer %d is going outside the Passport Office because there is a Senator present.\n", sizeof("Customer %d is going outside the Passport Office because there is a Senator present.\n"), c.ssn);
			Acquire(numCustomersLock);
			numCustomersOutside++;
			Release(numCustomersLock);
			while(numSenatorsPresent > 0) Wait(SenatorWaitCondition, SenatorLock); /*wait for senator to leave*/
			Acquire(numCustomersLock);
			numCustomersOutside--;
			Release(numCustomersLock);
		}else{

			/*Choose new clerk type RANDOMLY*/
			clerkChoice = Rand() % 4;
		}
		Release(SenatorLock);



		/* Choosing Application clerk*/
		if(!c.appAccepted && clerkChoice == 0){
			int myLine = -1;
			int lineSize = 10000;		
			bool bribe = false;
			int i;
			Acquire(appLineLock);	/*check app clerk line*/

			/*Decide whether to bribe*/
			if(c.cash >= 500){
				bribe = true;
			}

			for(i = 0; i < numAppClerks; i++) {		
				if (appClerks[i].state == available) {	/*Check for available clerks*/
					myLine = i;
					lineSize = appClerks[i].numLine;
					bribe = false;
					break;
				}
				else if(!bribe && appClerks[i].numLine < lineSize && appClerks[i].state != onbreak) { /*find shortest line*/
					myLine = i;
					lineSize = appClerks[i].numLine;
				}else if(bribe && appClerks[i].numBribeLine < lineSize && appClerks[i].state != onbreak) { /*find shortest line*/
					myLine = i;
					lineSize = appClerks[i].numBribeLine;
				}
			}
			if(myLine == -1) { /*No available lines*/
				Release(appLineLock);
				Yield();
				continue;
			}else if(numSenatorsPresent != 0){
				/*Check if they were taken out of line because of a senator*/
				appClerks[myLine].state = available;
				Release(appLineLock);
				continue;
			}
			else if(!bribe && appClerks[myLine].state == busy) { /*Get in line*/
				appClerks[myLine].numLine++;
				PrintfInt("Customer %d has gotten in regular line for ApplicationClerk %d\n", sizeof("Customer %d has gotten in regular line for ApplicationClerk %d\n"), 1000+1000*c.ssn + myLine);
				Wait(appClerks[myLine].waitCondition, appLineLock);
				appClerks[myLine].numLine--;
			}
			else if(bribe && appClerks[myLine].state == busy) { /*Get in line*/
				appClerks[myLine].numBribeLine++;
				PrintfInt("Customer %d has gotten in bribe line for ApplicationClerk %d\n", sizeof("Customer %d has gotten in bribe line for ApplicationClerk %d\n"), 1000+1000* c.ssn +  myLine);
				PrintfInt("ApplicationClerk %d has received $500 from Customer %d\n", sizeof("ApplicationClerk %d has received $500 from Customer %d\n"), 1000+1000*myLine + c.ssn);
				c.cash -= 500;
				appClerkCash += 500;
				Wait(appClerks[myLine].bribeWaitCondition, appLineLock);
				appClerks[myLine].numBribeLine--;
			}
			else {
				appClerks[myLine].state = busy;			/*If there is no one in line and the clerk is available they can go straight to the clerk*/
			}
			Release(appLineLock);
			/*Check if they were taken out of line because of a senator*/
			if(numSenatorsPresent != 0){
				continue;
			}
			Acquire(appClerks[myLine].ClerkLock);
			appClerks[myLine].dataIn = c.ssn;
			PrintfInt("Customer %d has given SSN %d to ApplicationClerk %d\n", sizeof("Customer %d has given SSN %d to ApplicationClerk %d\n"), 100000+100000*c.ssn + 1000+1000*c.ssn + myLine);
			Signal(appClerks[myLine].ClerkCV, appClerks[myLine].ClerkLock); /*Signal that ssn has been given*/
			Wait(appClerks[myLine].ClerkCV, appClerks[myLine].ClerkLock); /*Waiting for clerk to take SSN and file application*/
			c.appAccepted = true;
			Signal(appClerks[myLine].ClerkCV, appClerks[myLine].ClerkLock);/*Signal that customer is leaving counter*/
			Release(appClerks[myLine].ClerkLock);
		}
		
		/*/* Choosing Picture clerk*/
		if(!c.isPicTaken && clerkChoice == 1){
			/*Going to Picture Clerk*/
			int myLine = -1;
			int lineSize = 10000;	
			bool bribe = false;
			int i;
			int likesPic;
			Acquire(picLineLock);

			/*Decide whether to bribe*/
			if(c.cash >= 500){
				bribe = true;
			}	
			for(i = 0; i < numPicClerks; i++) {		
				if (picClerks[i].state == available) {	/*Check for available clerks*/
					myLine = i;
					lineSize = picClerks[i].numLine;
					bribe = false; /*do not bribe if clerk is available*/
					break;
				}
				else if(!bribe && picClerks[i].numLine < lineSize && picClerks[i].state != onbreak) { /*find shortest line*/
					myLine = i;
					lineSize = picClerks[i].numLine;
				}
				else if(bribe && picClerks[i].numBribeLine < lineSize && picClerks[i].state != onbreak) { /*find shortest line*/
					myLine = i;
					lineSize = picClerks[i].numBribeLine;
				}
			}
			if(myLine == -1) { /*No available lines*/
				Release(picLineLock);
				Yield();
				continue;
			}else if(numSenatorsPresent != 0){
				/*Check if they were taken out of line because of a senator*/
				picClerks[myLine].state = available;
				Release(picLineLock);
				continue;
			}
			else if(!bribe && picClerks[myLine].state == busy) { /*Get in line*/
				picClerks[myLine].numLine++;
				PrintfInt("Customer %d has gotten in regular line for PictureClerk %d\n", sizeof("Customer %d has gotten in regular line for PictureClerk %d\n"), 1000+1000*c.ssn + myLine);
				Wait(picClerks[myLine].waitCondition, picLineLock);
				picClerks[myLine].numLine--;
			}
			else if(bribe && picClerks[myLine].state == busy) { /*Get in bribe line*/
				picClerks[myLine].numBribeLine++;
				PrintfInt("Customer %d has gotten in bribe line for PictureClerk %d\n", sizeof("Customer %d has gotten in bribe line for PictureClerk %d\n"), 1000+1000* c.ssn + myLine);
				PrintfInt("PictureClerk %d has received $500 from Customer %d\n", sizeof("PictureClerk %d has received $500 from Customer %d\n"), 1000+1000* myLine + c.ssn);
				c.cash -= 500;
				picClerkCash += 500;
				Wait(picClerks[myLine].bribeWaitCondition, picLineLock);
				picClerks[myLine].numBribeLine--;
			}
			else {
				picClerks[myLine].state = busy;		/*If there is no one in line and the clerk is available they can go straight to the clerk*/
			}
			Release(picLineLock);
			/*Check if they were taken out of line because of a senator*/
			if(numSenatorsPresent != 0){
				continue;
			}
			Acquire(picClerks[myLine].ClerkLock);
			picClerks[myLine].dataIn = c.ssn;
			PrintfInt("Customer %d has given SSN %d to PictureClerk %d\n", sizeof("Customer %d has given SSN %d to PictureClerk %d\n"), 100000+ 100000*c.ssn + 1000 + 1000*c.ssn + myLine);
			Signal(picClerks[myLine].ClerkCV, picClerks[myLine].ClerkLock); /*Signal that ssn has been given*/
			Wait(picClerks[myLine].ClerkCV, picClerks[myLine].ClerkLock); /*Waiting for clerk to take SSN and take pic*/
			likesPic = Rand()%3;
			picClerks[myLine].dataIn = likesPic;
			if(likesPic == 0){ /*Did not like pic*/
				PrintfInt("Customer %d does not like their picture from PictureClerk %d\n", sizeof("Customer %d does not like their picture from PictureClerk %d\n"), 1000 + 1000* c.ssn + myLine);
			}else{ /*Likes Pic*/
				PrintfInt("Customer %d does like their picture from PictureClerk %d\n", sizeof("Customer %d does like their picture from PictureClerk %d\n"), 1000 + 1000 * c.ssn + myLine);
				c.isPicTaken = true;
			}
			Signal(picClerks[myLine].ClerkCV, picClerks[myLine].ClerkLock); /*signalling whether he likes picture or not*/
			Wait(picClerks[myLine].ClerkCV, picClerks[myLine].ClerkLock); /*Waiting for clerk to process pic or send customer away*/
			Release(picClerks[myLine].ClerkLock);
		}

		/* Choosing Passport clerk*/
		if(!c.appFiled && clerkChoice == 2) {
			int myLine = -1;
			int lineSize = 10000;	
			bool bribe = false;
			int i;
			Acquire(passLineLock);

			/*Decide whether to bribe*/
			if(c.cash >= 500){
				bribe = true;
			}	
			for(i = 0; i < numPassClerks; i++) {		
				if (passClerks[i].state == available) {	/*Check for available clerks*/
					myLine = i;
					lineSize = passClerks[i].numLine;
					bribe = false;
					break;
				}
				else if(!bribe && passClerks[i].numLine < lineSize && passClerks[i].state != onbreak) { /*find shortest line*/
					myLine = i;
					lineSize = passClerks[i].numLine;
				}
				else if(bribe && passClerks[i].numBribeLine < lineSize && passClerks[i].state != onbreak) { /*find shortest line*/
					myLine = i;
					lineSize = passClerks[i].numBribeLine;
				}
			}
			if(myLine == -1) { /*No available lines*/
				Release(passLineLock);
				Yield();
				continue;
			}else if(numSenatorsPresent != 0){
				/*Check if they were taken out of line because of a senator*/
				passClerks[myLine].state = available;
				Release(passLineLock);
				continue;
			}
			else if(!bribe && passClerks[myLine].state == busy) { /*Get in line*/
				passClerks[myLine].numLine++;
				PrintfInt("Customer %d has gotten in regular line for PassportClerk %d at position %d\n", sizeof("Customer %d has gotten in regular line for PassportClerk %d at position %d\n"), 100000+100000* c.ssn + 1000 + 1000 * myLine + passClerks[myLine].numLine);
				Wait(passClerks[myLine].waitCondition, passLineLock);
				passClerks[myLine].numLine--;
			}
			else if(bribe && passClerks[myLine].state == busy) { /*Get in bribe line*/
				passClerks[myLine].numBribeLine++;
				PrintfInt("Customer %d has gotten in bribe line for PassportClerk %d at position %d\n", sizeof("Customer %d has gotten in bribe line for PassportClerk %d at position %d\n"),100000+100000* c.ssn + 1000 + 1000* myLine + passClerks[myLine].numBribeLine);
				PrintfInt("PassportClerk %d has received $500 from Customer %d\n", sizeof("PassportClerk %d has received $500 from Customer %d\n"), 1000+ 1000* myLine + c.ssn);
				c.cash -= 500;
				passClerkCash += 500;
				Wait(passClerks[myLine].bribeWaitCondition, passLineLock);
				passClerks[myLine].numBribeLine--;
			}
			else {
				passClerks[myLine].state = busy;			/*If there is no one in line and the clerk is available they can go straight to the clerk*/
			}
			Release(passLineLock);

			if(numSenatorsPresent != 0){
				continue;
			}
			Acquire(passClerks[myLine].ClerkLock);
			passClerks[myLine].dataIn = c.ssn;
			PrintfInt("Customer %d has given SSN %d to PassportClerk %d\n", sizeof("Customer %d has given SSN %d to PassportClerk %d\n"), 100000+100000* c.ssn + 1000 + 1000* c.ssn + myLine);
			Signal(passClerks[myLine].ClerkCV, passClerks[myLine].ClerkLock); /*Signal that ssn has been given*/
			Wait(passClerks[myLine].ClerkCV, passClerks[myLine].ClerkLock);
			if(!(c.appAccepted && c.isPicTaken)) {
				int waitTime = Rand()%901 + 100;
				int i;
				PrintfInt("Customer %d has gone to PassportClerk %d too soon. They are going to the back of the line.\n", sizeof("Customer %d has gone to PassportClerk %d too soon. They are going to the back of the line.\n"), 1000+1000* c.ssn + myLine);
				Release(passClerks[myLine].ClerkLock);
				for(i = 0; i < waitTime; i++) {
					Yield();
				}
			}
			else {
				c.appFiled = true;
				Release(passClerks[myLine].ClerkLock);
			}
		}

		/* Choosing Cashier*/
		if(!c.hasPassport && clerkChoice == 3) {
			int myLine = -1;
			int lineSize = 10000;	
			bool bribe = false;
			int i;
			Acquire(cashLineLock);

			/*Decide whether to bribe*/
			if(c.cash >= 500){
				bribe = true;
			}	
			for(i = 0; i < numCashClerks; i++) {		
				if (cashClerks[i].state == available) {	/*Check for available clerks*/
					myLine = i;
					lineSize = cashClerks[i].numLine;
					bribe = false;
					break;
				}
				else if(!bribe && cashClerks[i].numLine < lineSize && cashClerks[i].state != onbreak) { /*find shortest line*/
					myLine = i;
					lineSize = cashClerks[i].numLine;
				}
				else if(bribe && cashClerks[i].numBribeLine < lineSize && cashClerks[i].state != onbreak) { /*find shortest line*/
					myLine = i;
					lineSize = cashClerks[i].numBribeLine;
				}
			}
			if(myLine == -1) { /*No available lines*/
				Release(cashLineLock);
				Yield();
				continue;
			}else if(numSenatorsPresent != 0){
				cashClerks[myLine].state = available;
				Release(cashLineLock);
				continue;
			}
			else if(!bribe && cashClerks[myLine].state == busy) { /*Get in line*/
				cashClerks[myLine].numLine++;
				PrintfInt("Customer %d has gotten in regular line for Cashier %d at position %d\n", sizeof("Customer %d has gotten in regular line for Cashier %d at position %d\n"), 100000+100000* c.ssn + 1000 + 1000* myLine + cashClerks[myLine].numLine);
				Wait(cashClerks[myLine].waitCondition, cashLineLock);
				cashClerks[myLine].numLine--;
			}
			else if(bribe && cashClerks[myLine].state == busy) { /*Get in bribe line*/
				cashClerks[myLine].numBribeLine++;
				PrintfInt("Customer %d has gotten in bribe line for Cashier %d at position %d\n", sizeof("Customer %d has gotten in bribe line for Cashier %d at position %d\n"),100000+100000* c.ssn + 1000+1000* myLine + cashClerks[myLine].numBribeLine);
				PrintfInt("Cashier %d has received $500 from Customer %d\n", sizeof("Cashier %d has received $500 from Customer %d\n"), 1000+1000* myLine + c.ssn);
				c.cash -= 500;
				Acquire(cashCashLock);
				cashClerkCash += 500;
				Release(cashCashLock);
				Wait(cashClerks[myLine].bribeWaitCondition, cashLineLock);
				cashClerks[myLine].numBribeLine--;
			}
			else {
				cashClerks[myLine].state = busy;		/*If there is no one in line and the clerk is available they can go straight to the clerk*/
			}
			if(numSenatorsPresent != 0){
				continue;
			}
			Release(cashLineLock);
			/*Check if they were taken out of line because of a senator*/
			Acquire(cashClerks[myLine].ClerkLock);
			cashClerks[myLine].dataIn = c.ssn;
			PrintfInt("Customer %d has given SSN %d to Cashier %d\n", sizeof("Customer %d has given SSN %d to Cashier %d\n"), 100000+100000* c.ssn + 1000 + 1000* c.ssn + myLine);
			if(!c.hasPaidCashier) {		/*pay now if they havent paid yet*/
				c.cash -= 100;
				c.hasPaidCashier = true;
				PrintfInt("Customer %d has given Cashier %d 100$.\n", sizeof("Customer %d has given Cashier %d 100$.\n"), 1000 + 1000* c.ssn + myLine);
			}
			Signal(cashClerks[myLine].ClerkCV, cashClerks[myLine].ClerkLock); /*Signal that ssn and money has been given*/			
			Wait(cashClerks[myLine].ClerkCV, cashClerks[myLine].ClerkLock);
			if(!(c.appAccepted && c.isPicTaken && c.appFiled)) {
				int waitTime = Rand()%901 + 100;
				int i;
				PrintfInt("Customer %d has gone to Cashier %d too soon. They are going to the back of the line.\n", sizeof("Customer %d has gone to Cashier %d too soon. They are going to the back of the line.\n"), 1000 + 1000*c.ssn + myLine);
				Release(cashClerks[myLine].ClerkLock);
				for(i = 0; i < waitTime; i++) {
					Yield();
				}
			}
			else {
				c.hasPassport = true;						/*The customer has received their passport*/
				Acquire(numCustomersLock);
				numCustomersWhoHaveLeftTheOffice++;
				Release(numCustomersLock);
				PrintfInt("Customer %d is leaving the Passport Office.\n", sizeof("Customer %d is leaving the Passport Office.\n"),  c.ssn);			/*They can now leave the office and end their thread*/
				Release(cashClerks[myLine].ClerkLock);
			}
		}
	}
	Exit(0);
}

void SenatorThread(int index){
	
	/*Wait random time before entering office*/
	int waitTime;
	int i;
	int myLine;
	Customer s;
	Acquire(SenatorLock);
	s.ssn = senatorIndex + numCustomers;
	senatorIndex++;
	Release(SenatorLock);
	s.appAccepted = false;
	s.isPicTaken = false;
	s.appFiled = false;
	s.hasPassport = false;
	s.hasPaidCashier = false;
	s.cash = (Rand()%4)*500 + 100;
	waitTime = Rand() % 901 + 100;
	for(i = 0; i < waitTime; i++) Yield();
	Acquire(SenatorLock);
	numSenatorsPresent++;

	if(numSenatorsPresent > 1){ /*not the first senator, must wait outside*/
		Wait(SenatorWaitCondition, SenatorLock);
	}

	/*Signals everyone to leave lines*/
	Acquire(appLineLock);
	for(i = 0; i < numAppClerks; i++){
		if(appClerks[i].numLine > 0) Broadcast(appClerks[i].waitCondition, appLineLock);
		if(appClerks[i].numBribeLine > 0) Broadcast(appClerks[i].bribeWaitCondition, appLineLock);
	}
	Release(appLineLock);
	Acquire(picLineLock);
	for(i = 0; i < numPicClerks; i++){
		if(picClerks[i].numLine > 0)  Broadcast(picClerks[i].waitCondition, picLineLock);
		if(picClerks[i].numBribeLine > 0)  Broadcast(picClerks[i].bribeWaitCondition, picLineLock);
	}
	Release(picLineLock);
	Acquire(passLineLock);
	for(i = 0; i < numPassClerks; i++){
		if(passClerks[i].numLine > 0)  Broadcast(passClerks[i].waitCondition, passLineLock);
		if(passClerks[i].numBribeLine > 0) Broadcast(passClerks[i].bribeWaitCondition, passLineLock);
	}
	Release(passLineLock);
	Acquire(cashLineLock);
	for(i = 0; i < numCashClerks; i++){
		if(picClerks[i].numLine > 0)  Broadcast(cashClerks[i].waitCondition, cashLineLock);
		if(cashClerks[i].numBribeLine > 0)  Broadcast(cashClerks[i].bribeWaitCondition, cashLineLock);
	}
	Release(cashLineLock);

	/*Actively checking until all customers have left*/

	while(numCustomersOutside < numCustomers - numCustomersWhoHaveLeftTheOffice) {
		Release(SenatorLock);
		Yield();	
		Acquire(SenatorLock);
	}

	/*Application Clerk*/
	Acquire(appLineLock);
	myLine = -1;
	for(i = 0; i < numAppClerks; i++) {
		if(appClerks[i].state == available) {
			myLine = i;
			PrintfInt("Senator %d has gotten in regular line for ApplicationClerk %d\n", sizeof("Senator %d has gotten in regular line for ApplicationClerk %d\n"), 1000+1000* s.ssn + myLine);
			break;
		}
	}
	appClerks[myLine].state = busy;
	Release(appLineLock);

	Acquire(appClerks[myLine].ClerkLock);
	appClerks[myLine].dataIn = s.ssn;
	PrintfInt("Senator %d has given SSN %d to ApplicationClerk %d\n", sizeof("Senator %d has given SSN %d to ApplicationClerk %d\n"), 100000 + 100000 * s.ssn + 1000 + 1000* s.ssn + myLine);
	Signal(appClerks[myLine].ClerkCV, appClerks[myLine].ClerkLock); /*Signal that ssn has been given*/
	Wait(appClerks[myLine].ClerkCV, appClerks[myLine].ClerkLock); /*Waiting for clerk to take SSN and file application*/
	s.appAccepted = true;
	Signal(appClerks[myLine].ClerkCV, appClerks[myLine].ClerkLock);/*Signal that customer is leaving counter*/
	Release(appClerks[myLine].ClerkLock);

	/*Picture Clerk*/
	while(!s.isPicTaken) {
		int likesPic;
		Acquire(picLineLock);
		myLine = -1;
		for(i = 0; i < numPicClerks; i++) {
			if(picClerks[i].state == available) {
				myLine = i;
				PrintfInt("Senator %d has gotten in regular line for PictureClerk %d\n", sizeof("Senator %d has gotten in regular line for PictureClerk %d\n"), 1000+1000* s.ssn + myLine);
				break;
			}
		}
		if(myLine == -1){
			Release(picLineLock);
			Yield();
			continue;
		}
		picClerks[myLine].state = busy;
		Release(picLineLock);
		Acquire(picClerks[myLine].ClerkLock);
		picClerks[myLine].dataIn = s.ssn;
		PrintfInt("Senator %d has given SSN %d to PictureClerk %d\n", sizeof("Senator %d has given SSN %d to PictureClerk %d\n"), 100000+100000* s.ssn + 1000 + 1000* s.ssn + myLine);
		Signal(picClerks[myLine].ClerkCV, picClerks[myLine].ClerkLock); /*Signal that ssn has been given*/
		Wait(picClerks[myLine].ClerkCV, picClerks[myLine].ClerkLock); /*Waiting for clerk to take SSN and take pic*/
		likesPic = Rand()%4;
		picClerks[myLine].dataIn = likesPic;
		if(likesPic == 0){ /*Did not like pic*/
			PrintfInt("Senator %d does not like their picture from PictureClerk %d\n", sizeof("Senator %d does not like their picture from PictureClerk %d\n"), 1000+1000* s.ssn + myLine);
		}else{ /*Likes Pic*/
			PrintfInt("Senator %d does like their picture from PictureClerk %d\n", sizeof("Senator %d does like their picture from PictureClerk %d\n"), 1000+ 1000* s.ssn + myLine);
			s.isPicTaken = true;
		}
		Signal(picClerks[myLine].ClerkCV, picClerks[myLine].ClerkLock); /*signalling whether he likes picture or not*/
		Wait(picClerks[myLine].ClerkCV, picClerks[myLine].ClerkLock); /*Waiting for clerk to process pic or send customer away*/
		Release(picClerks[myLine].ClerkLock);
	}

	/*Passport clerk*/
	while(!s.appFiled) {
		Acquire(passLineLock);
		myLine = -1;
		for( i = 0; i < numPassClerks; i++) {
			if(passClerks[i].state == available) {
				myLine = i;
				PrintfInt("Senator %d has gotten in regular line for PassportClerk %d\n", sizeof("Senator %d has gotten in regular line for PassportClerk %d\n"), 1000+1000* s.ssn + myLine);
				break;
			}
		}
		if(myLine == -1){
			Release(passLineLock);
			Yield();
			continue;
		}
		passClerks[myLine].state = busy;
		Release(passLineLock);

		Acquire(passClerks[myLine].ClerkLock);
		passClerks[myLine].dataIn = s.ssn;
		PrintfInt("Senator %d has given SSN %d to PassportClerk %d\n", sizeof("Senator %d has given SSN %d to PassportClerk %d\n"), 100000 + 100000* s.ssn + 1000 + 1000* s.ssn + myLine);
		Signal(passClerks[myLine].ClerkCV, passClerks[myLine].ClerkLock); /*Signal that ssn has been given*/
		Wait(passClerks[myLine].ClerkCV, passClerks[myLine].ClerkLock); /*Waiting for clerk to take SSN and file application*/
		s.appFiled = true;
		Release(passClerks[myLine].ClerkLock);
	}

	/*Cashier*/
	while(!s.hasPassport) {

		Acquire(cashLineLock);
		myLine = -1;
		for(i = 0; i < numCashClerks; i++) {
			if(cashClerks[i].state == available) {
				myLine = i;
				PrintfInt("Senator %d has gotten in regular line for Cashier %d\n", sizeof("Senator %d has gotten in regular line for Cashier %d\n"), 1000+1000* s.ssn + myLine);
				break;
			}
		}
		if(myLine == -1){
			Release(cashLineLock);
			Yield();
			continue;
		}
		cashClerks[myLine].state = busy;
		Release(cashLineLock);
		Acquire(cashClerks[myLine].ClerkLock);
		cashClerks[myLine].dataIn = s.ssn;
		PrintfInt("Senator %d has given SSN %d to Cashier %d\n", sizeof("Senator %d has given SSN %d to Cashier %d\n"), 100000 + 100000* s.ssn + 1000 + 1000 * s.ssn + myLine);
		if(!s.hasPaidCashier) {			/*pay now if they havent paid yet  */
			s.cash -= 100;
			s.hasPaidCashier = true;
			PrintfInt("Senator %d has given Cashier %d 100$.\n", sizeof("Senator %d has given Cashier %d 100$.\n"), 1000 + 1000*s.ssn + myLine);
		}
		Signal(cashClerks[myLine].ClerkCV, cashClerks[myLine].ClerkLock); /*Signal that ssn and money has been given		*/	
		Wait(cashClerks[myLine].ClerkCV, cashClerks[myLine].ClerkLock); /* Wait to receive passport (they will always go in the right )*/
		if(!(s.appAccepted && s.isPicTaken && s.appFiled)) {
			PrintfInt("ERROR Senator %d has gone to cashier %d before other clerks, this shouldn't be possible.", sizeof("ERROR Senator %d has gone to cashier %d before other clerks, this shouldn't be possible."), 1000 + 1000*s.ssn + myLine);
		}
		s.hasPassport = true;
		Release(cashClerks[myLine].ClerkLock);
	}
	numSenatorsPresent--;
	PrintfInt("Senator %d is leaving the Passport Office.\n", sizeof("Senator %d is leaving the Passport Office.\n"), s.ssn);
	Broadcast(SenatorWaitCondition, SenatorLock); /*Let customers (and potentially other senator) know that he has left so they can enter*/
	Release(SenatorLock);
	Acquire(numCustomersLock);
	numSenatorsWhoHaveLeftTheOffice++;
	Release(numCustomersLock);
	Exit(0);
}

/*Application Clerk thread*/
void AppClerkThread() {
	char *name;
	int waitTime;
	int i;
	Clerk* ac;
	int index;

	Acquire(appLineLock);
	index = appIndex;
	ac = &appClerks[appIndex++];
	Release(appLineLock);
	
	while(true){

		/*End of simulation condition*/
		Acquire(numCustomersLock);
		if(numCustomersWhoHaveLeftTheOffice + numSenatorsWhoHaveLeftTheOffice == numCustomers + numSenators) {
			Release(numCustomersLock);
			break;
		}
		Release(numCustomersLock);
		Acquire(appLineLock);
		if(ac->numBribeLine > 0){
			PrintfInt("ApplicationClerk %d has signalled a customer to come to their counter\n", sizeof("ApplicationClerk %d has signalled a customer to come to their counter\n"), index);
			Signal(ac->bribeWaitCondition, appLineLock);
			ac->state = busy;		
		}else if(ac->numLine > 0){
			PrintfInt("ApplicationClerk %d has signalled a customer to come to their counter\n", sizeof("ApplicationClerk %d has signalled a customer to come to their counter\n"), index);
			Signal(ac->waitCondition, appLineLock);
			ac->state = busy;
		}else if(numAppClerksAwake > 1 && ac->servedOne){				/*If there is no one in line, there is atleast one other clerk of their type working and they have served a customer then they can go on break*/
			ac->state = onbreak;
			PrintfInt("ApplicationClerk %d is going on break\n", sizeof("ApplicationClerk %d is going on break\n"), index);
			numAppClerksAwake--;
			Wait(appBreakCondition, appLineLock);
			numAppClerksAwake++;
			PrintfInt("ApplicationClerk %d is coming off break\n", sizeof("ApplicationClerk %d is coming off break\n"), index);
			ac->state = available;
			ac->servedOne = false;
		}
		else{
			ac->state = available;							/*If there is no one in line they set themselves to available*/
		}
		Acquire(ac->ClerkLock);
		Release(appLineLock);
		if(numSenatorsPresent != 0) {
			ac->state = available;
		}
		Wait(ac->ClerkCV, ac->ClerkLock); /*Wait for customer to give ssn*/
		customersDatabase[ac->dataIn].ssn = ac->dataIn;
		PrintfInt("ApplicationClerk %d has received a SSN %d from Customer %d\n", sizeof("ApplicationClerk %d has received a SSN %d from Customer %d\n"), 100000+ 100000* index + 1000 + 1000 * ac->dataIn + ac->dataIn);
		waitTime = Rand() % 81 + 20;
		for(i = 0; i < waitTime; i++) Yield(); /*Process for random amount of time(yields)*/
		customersDatabase[ac->dataIn].appAccepted = true;
		PrintfInt("ApplicationClerk %d has recorded a completed application for Customer %d\n", sizeof("ApplicationClerk %d has recorded a completed application for Customer %d\n"), 1000+1000* index + ac->dataIn);
		Signal(ac->ClerkCV, ac->ClerkLock); /*Signal that application has been filed*/
		Wait(ac->ClerkCV, ac->ClerkLock); /*Wait for customer to leave the counter*/
		ac->servedOne = true;
		Release(ac->ClerkLock);
	}
	Exit(0);
}

/*Picture clerk thread*/
void PicClerkThread(){
	char* name;
	int waitTime;
	int customerID;
	int i;
	int index;
	Clerk* pc;
	Acquire(picLineLock);
	index = picIndex;
	pc = &picClerks[picIndex++];
	Release(picLineLock);

	while(true){
		/*End of simulation condition*/
		Acquire(numCustomersLock);
		if(numCustomersWhoHaveLeftTheOffice + numSenatorsWhoHaveLeftTheOffice == numCustomers + numSenators) {
			Release(numCustomersLock);
			break;
		}
		Release(numCustomersLock);
		Acquire(picLineLock);
		if(pc->numBribeLine > 0){
			PrintfInt("PictureClerk %d has signalled a customer to come to their counter\n", sizeof("PictureClerk %d has signalled a customer to come to their counter\n"), index);
			Signal(pc->bribeWaitCondition, picLineLock);
			pc->state = busy;		
		}else if(pc->numLine > 0){
			PrintfInt("PictureClerk %d has signalled a customer to come to their counter\n", sizeof("PictureClerk %d has signalled a customer to come to their counter\n"), index);
			Signal(pc->waitCondition, picLineLock);
			pc->state = busy;
		}else if(numPicClerksAwake > 1 && pc->servedOne){				/*If there is no one in line, there is atleast one other clerk of their type working and they have served a customer then they can go on break*/
			pc->state = onbreak;
			PrintfInt("PictureClerk %d is going on break\n", sizeof("PictureClerk %d is going on break\n"), index);
			numPicClerksAwake--;
			Wait(picBreakCondition, picLineLock);
			numPicClerksAwake++;
			PrintfInt("PictureClerk %d is coming off break\n", sizeof("PictureClerk %d is coming off break\n"), index);
			pc->state = available;
			pc->servedOne = false;
		}
		else{
			pc->state = available;			/*If there is no one in line they set themselves to available*/
		}
		Acquire(pc->ClerkLock);
		Release(picLineLock);
		if(numSenatorsPresent != 0) {
			pc->state = available;
		}
		Wait(pc->ClerkCV, pc->ClerkLock); /*Wait for customer to give ssn*/
		customerID = pc->dataIn;
		customersDatabase[customerID].ssn = customerID;
		PrintfInt("PictureClerk %d has received a SSN %d from Customer %d\n", sizeof("PictureClerk %d has received a SSN %d from Customer %d\n"), 100000 + 100000* index + 1000 + 1000* customerID + customerID);
		waitTime = Rand() % 41 + 10;
		for(i = 0; i < waitTime; i++) Yield(); /*Take picture for random amount of time(yields)*/
		Signal(pc->ClerkCV, pc->ClerkLock); /*Signal to customer picture was taken*/
		Wait(pc->ClerkCV, pc->ClerkLock); /*Wait for customer to decide if they like it*/
		/*if they did not like pic (and assumed to have left the counter)*/
		if(pc->dataIn == 0){
			PrintfInt("PictureClerk %d has been told that Customer %d does not like their picture\n", sizeof("PictureClerk %d has been told that Customer %d does not like their picture\n"), 1000 + 1000* index + customerID);

		}/*if they liked pic*/
		else{
			PrintfInt("PictureClerk %d has been told that Customer %d does like their picture\n", sizeof("PictureClerk %d has been told that Customer %d does like their picture\n"), 1000 + 1000* index + customerID);
			waitTime = Rand() % 81 + 20;
			for(i = 0; i < waitTime; i++) Yield(); /*Process picture for random amount of time(yields)*/
				customersDatabase[customerID].isPicTaken = true;
		}
		Signal(pc->ClerkCV, pc->ClerkLock); /*Signal to customer picture was processed or that the customer can get back in line*/
		pc->servedOne = true;

		Release(pc->ClerkLock);
	}
	Exit(0);
}

/*Passport Clerk Thread*/
void PassClerkThread() {
	char* name;
	int waitTime;
	int customerID;
	int i;
	int index;
	Clerk* ptc;
	Acquire(passLineLock);
	index = passIndex;
	ptc = &passClerks[passIndex++];
	Release(passLineLock);
	while(true) {
		
		/*End of simulation condition*/
		Acquire(numCustomersLock);
		if(numCustomersWhoHaveLeftTheOffice + numSenatorsWhoHaveLeftTheOffice == numCustomers + numSenators) {
			Release(numCustomersLock);
			break;
		}
		Release(numCustomersLock);
		Acquire(passLineLock);
		if(ptc->numBribeLine > 0){
			PrintfInt("PassportClerk %d has signalled a customer to come to their counter\n", sizeof("PassportClerk %d has signalled a customer to come to their counter\n"), index);
			Signal(ptc->bribeWaitCondition, passLineLock);
			ptc->state = busy;		
		}else if(ptc->numLine > 0){
			PrintfInt("PassportClerk %d has signalled a customer to come to their counter\n", sizeof("PassportClerk %d has signalled a customer to come to their counter\n"), index);
			Signal(ptc->waitCondition, passLineLock);
			ptc->state = busy;
		}else if(numPassClerksAwake > 1 && ptc->servedOne){							/*If there is no one in line, there is atleast one other clerk of their type working and they have served a customer then they can go on break*/
			ptc->state = onbreak;
			PrintfInt("PassportClerk %d is going on break\n", sizeof("PassportClerk %d is going on break\n"), index);
			numPassClerksAwake--;
			Wait(passBreakCondition, passLineLock);
			numPassClerksAwake++;
			PrintfInt("PassportClerk %d is coming off break\n", sizeof("PassportClerk %d is coming off break\n"), index);
			ptc->state = available;
			ptc->servedOne = false;
		}
		else{
			ptc->state = available;							/*If there is no one in line they set themselves to available*/
		}
		Acquire(ptc->ClerkLock);
		Release(passLineLock);
		if(numSenatorsPresent != 0) {
			ptc->state = available;
		}
		Wait(ptc->ClerkCV, ptc->ClerkLock); /*Wait for customer to give ssn*/
		customerID = ptc->dataIn;
		PrintfInt("PassportClerk %d has received a SSN %d from Customer %d\n", sizeof("PassportClerk %d has received a SSN %d from Customer %d\n"), 100000+ 100000 *index + 1000 + 1000 * customerID + customerID);
		if(!(customersDatabase[customerID].isPicTaken && customersDatabase[customerID].appAccepted)) {
			PrintfInt("PassportClerk %d has determined that Customer %d does not have both their application and picture completed\n", sizeof("PassportClerk %d has determined that Customer %d does not have both their application and picture completed\n"), 1000 + 1000* index + customerID);
		} 
		else {
			PrintfInt("PassportClerk %d has determined that Customer %d has both their application and picture completed\n", sizeof("PassportClerk %d has determined that Customer %d has both their application and picture completed\n"), 1000 + 1000* index + customerID);
			waitTime = Rand() % 81 + 20;
			for(i = 0; i < waitTime; i++) Yield(); /*Recording customer documents*/
				customersDatabase[customerID].appFiled = true;
			PrintfInt("PassportClerk %d has has recorded Customer %d passport documentation\n", sizeof("PassportClerk %d has has recorded Customer %d passport documentation\n"), 1000 + 1000* index + customerID);
		}
		Signal(ptc->ClerkCV, ptc->ClerkLock);
		ptc->servedOne = true;
		Release(ptc->ClerkLock);
	}
	Exit(0);
}

/*Cashier Thread*/
void CashClerkThread() {
	char* name;
	int waitTime;
	int i;
	int customerID;
	int index;
	Clerk* cc;
	Acquire(cashLineLock);
	index = cashIndex;
	cc = &cashClerks[cashIndex++];
	Release(cashLineLock);
	while(true) {
		
		/*End of simulation condition*/
		Acquire(numCustomersLock);
		if(numCustomersWhoHaveLeftTheOffice + numSenatorsWhoHaveLeftTheOffice == numCustomers + numSenators) {
			Release(numCustomersLock);
			break;
		}
		Release(numCustomersLock);
		Acquire(cashLineLock);
		if(cc->numBribeLine > 0){
			PrintfInt("Cashier %d has signalled a customer to come to their counter\n", sizeof("Cashier %d has signalled a customer to come to their counter\n"), index);
			Signal(cc->bribeWaitCondition, cashLineLock);
			cc->state = busy;		
		}else if(cc->numLine > 0){
			PrintfInt("Cashier %d has signalled a customer to come to their counter\n", sizeof("Cashier %d has signalled a customer to come to their counter\n"), index);
			Signal(cc->waitCondition, cashLineLock);
			cc->state = busy;
		}else if(numCashClerksAwake > 1 && cc->servedOne){							/*If there is no one in line, there is atleast one other clerk of their type working and they have served a customer then they can go on break*/
			cc->state = onbreak;
			PrintfInt("Cashier %d is going on break\n", sizeof("Cashier %d is going on break\n"), index);
			numCashClerksAwake--;
			Wait(cashBreakCondition, cashLineLock);
			numCashClerksAwake++;
			PrintfInt("Cashier %d is coming off break\n", sizeof("Cashier %d is coming off break\n"), index);
			cc->state = available;
			cc->servedOne = false;
		}
		else{
			cc->state = available;				/*If there is no one in line they set themselves to available*/
		}

		Acquire(cc->ClerkLock);
		Release(cashLineLock);

		if(numSenatorsPresent != 0) {
			cc->state = available;
		}

		Wait(cc->ClerkCV, cc->ClerkLock); /*Wait for customer to give ssn*/
		customerID = cc->dataIn;
		PrintfInt("Cashier %d has received a SSN %d from Customer %d\n", sizeof("Cashier %d has received a SSN %d from Customer %d\n"), 100000+ 100000* index + 1000 + 1000 * customerID + customerID);
		if((customersDatabase[customerID].isPicTaken && customersDatabase[customerID].appAccepted && customersDatabase[customerID].appFiled)) {		/*checking if the customer has been certified yet*/
			PrintfInt("Cashier %d has verified that Customer %d has been certified by a PassportClerk\n", sizeof("Cashier %d has verified that Customer %d has been certified by a PassportClerk\n"), 1000 + 1000* index + customerID);	
			if(!customersDatabase[customerID].hasPaidCashier) {
				PrintfInt("Cashier %d has received the $100 from Customer %d after certification.\n", sizeof("Cashier %d has received the $100 from Customer %d after certification.\n"), 1000 + 1000* index + customerID);
				Acquire(cashCashLock);
				cashClerkCash += 100;
				Release(cashCashLock);
				customersDatabase[customerID].hasPaidCashier = true;
			}
			waitTime = Rand() % 81 + 20;
			for(i = 0; i < waitTime; i++) Yield(); /*Recording customer documents*/
			PrintfInt("Cashier %d has provided Customer %d their completed passport\n", sizeof("Cashier %d has provided Customer %d their completed passport\n"), 1000 + 1000* index + customerID);
			customersDatabase[customerID].hasPassport = true;
		}
		else if(!customersDatabase[customerID].hasPaidCashier) {
			PrintfInt("Cashier %d has received the $100 from Customer %d before certification. They are to go to the back of my line.\n", sizeof("Cashier %d has received the $100 from Customer %d before certification. They are to go to the back of my line.\n"), 1000 + 1000* index + customerID);			
			Acquire(cashCashLock);
			cashClerkCash += 100;				/*Add the money to the clerk total*/
			Release(cashCashLock);
			customersDatabase[customerID].hasPaidCashier = true;
		}
		Signal(cc->ClerkCV, cc->ClerkLock);
		if(customersDatabase[customerID].hasPassport){
			PrintfInt("Cashier %d has recorded that Customer %d has been given their completed passport\n", sizeof("Cashier %d has recorded that Customer %d has been given their completed passport\n"), 1000 + 1000* index + customerID);
		}
		cc->servedOne = true;
		Release(cc->ClerkLock);
	}
	Exit(0);
}

void ManagerThread(int index) {
	int iterations = 0, total = 0, printCount = 10;
	while(true) {
		int i, in;
		total = 0;

		Acquire(appLineLock);
		for(i = 0; i < numAppClerks; i++) {
			if(appClerks[i].numLine >= 3 || appClerks[i].numBribeLine >= 3 || numAppClerksAwake == 0) {		/*check to see if there are more than 3 people in any of the lines*/
				for(in = 0; in < numAppClerks; in++) {	
					if(appClerks[in].state == onbreak) {				/*if the clerk is on break wake them up				*/
						Signal(appBreakCondition, appLineLock);
						Write("Manager has woken up an ApplicationClerk\n", sizeof("Manager has woken up an ApplicationClerk\n"), ConsoleOutput);
						break;
					}
				}
				break;
			}
		}
		total += appClerkCash;
		if(iterations >= printCount) PrintLargeInt("Manager has counted a total of $%d for ApplicationClerks\n", sizeof("Manager has counted a total of $%d for ApplicationClerks\n"), appClerkCash);
		Release(appLineLock);
		Acquire(picLineLock);
		for(i = 0; i < numPicClerks; i++) {
			if(picClerks[i].numLine >= 3 || picClerks[i].numBribeLine >= 3 || numPicClerksAwake == 0) {		/*check to see if there are more than 3 people in any of the lines*/
				for(in = 0; in < numPicClerks; in++) {
					if(picClerks[in].state == onbreak) {				/*if the clerk is on break wake them up		*/
						Signal(picBreakCondition, picLineLock);
						Write("Manager has woken up a PictureClerk\n", sizeof("Manager has woken up a PictureClerk\n"), ConsoleOutput);
						break;
					}
				}
				break;
			}
		}
		total += picClerkCash;
		if(iterations >= printCount) PrintLargeInt("Manager has counted a total of $%d for PictureClerks\n", sizeof("Manager has counted a total of $%d for PictureClerks\n"), picClerkCash);
		Release(picLineLock);
		Acquire(passLineLock);
		for(i = 0; i < numPassClerks; i++) {

			if(passClerks[i].numLine >= 3 || passClerks[i].numBribeLine >= 3 || numPassClerksAwake == 0) {		/*check to see if there are more than 3 people in any of the lines*/
				for(in = 0; in < numPassClerks; in++) {
					if(passClerks[in].state == onbreak) {				/*if the clerk is on break wake them up		*/
						Signal(passBreakCondition, passLineLock);
						Write("Manager has woken up a PassportClerk\n", sizeof("Manager has woken up a PassportClerk\n"), ConsoleOutput);
						break;
					}
				}
				break;
			}
		}
		total += passClerkCash;
		if(iterations >= printCount) PrintLargeInt("Manager has counted a total of $%d for PassportClerks\n", sizeof("Manager has counted a total of $%d for PassportClerks\n"), passClerkCash);
		Release(passLineLock);
		Acquire(cashLineLock);
		for(i = 0; i < numCashClerks; i++) {
			if(cashClerks[i].numLine >= 3 || cashClerks[i].numBribeLine >= 3 || numCashClerksAwake == 0) {		/*check to see if there are more than 3 people in any of the lines*/
				for(in = 0; in < numCashClerks; in++) {
					if(cashClerks[in].state == onbreak) {				/*if the clerk is on break wake them up		*/
						Signal(cashBreakCondition, cashLineLock);
						Write("Manager has woken up a Cashier\n", sizeof("Manager has woken up a Cashier\n"), ConsoleOutput);
						break;
					}
				}
				break;
			}
		}
		Acquire(cashCashLock);
		total += cashClerkCash;
		if(iterations >= printCount) PrintLargeInt("Manager has counted a total of $%d for Cashiers\n", sizeof("Manager has counted a total of $%d for Cashiers\n"), cashClerkCash);		/*lock when calculating cash to avoid race conditions*/
		Release(cashCashLock);
		Release(cashLineLock);
		for(i = 0; i < 35; i++) {
			Yield();
		}
		if(iterations >= printCount) {
			PrintLargeInt("Manager has counted a total of $%d for the passport office\n", sizeof("Manager has counted a total of $%d for the passport office\n"), total);		/*printing total money made at the passport office*/
			iterations = 0;
		}
		
		/*End of simulation condition*/
		Acquire(numCustomersLock);
		if(numCustomersWhoHaveLeftTheOffice + numSenatorsWhoHaveLeftTheOffice == numCustomers + numSenators) {
			Release(numCustomersLock);
			break;
		}
		Release(numCustomersLock);
		iterations++;
	}
	Exit(0);
}


void StartSimulation(){
	int i;
	numAppClerksAwake = numAppClerks;
	numPicClerksAwake = numPicClerks;
	numPassClerksAwake = numPassClerks;
	numCashClerksAwake = numCashClerks;
	for(i = 0; i < numAppClerks; i++) {
		Fork(AppClerkThread);
	}

	for(i = 0; i < numPicClerks; i++) {
		Fork(PicClerkThread);
	}

	for(i = 0; i < numPassClerks; i++) {
		Fork(PassClerkThread);
	}

	for(i = 0; i < numCashClerks; i++) {
		Fork(CashClerkThread);
	}

	Fork(ManagerThread);

	for(i = 0; i < numCustomers; i++) {
		Fork(CustomerThread);
	}

	for(i = 0; i < numSenators; i++) {
		Fork(SenatorThread);
	}

}

/*Part 2 of the project gives user choice of test to run*/
void main() {
	
	Write("Enter the number of the test you would like to run\n", sizeof("Enter the number of the test you would like to run\n"), ConsoleOutput);
	Write("Test 1: Customers always take the shortest line, but no 2 customers ever choose the same shortest line at the same time\n", sizeof("Test 1: Customers always take the shortest line, but no 2 customers ever choose the same shortest line at the same time\n"), ConsoleOutput);
	Write("Test 2: Managers only read one from one Clerk's total money received, at a time.\n", sizeof("Test 2: Managers only read one from one Clerk's total money received, at a time.\n"), ConsoleOutput);
	Write("Test 3: Customers do not leave until they are given their passport by the Cashier. The Cashier does not start on another customer until they know that the last Customer has left their area\n", sizeof("Test 3: Customers do not leave until they are given their passport by the Cashier. The Cashier does not start on another customer until they know that the last Customer has left their area\n"), ConsoleOutput);
	Write("Test 4: Clerks go on break when they have no one waiting in their line\n", sizeof("Test 4: Clerks go on break when they have no one waiting in their line\n"), ConsoleOutput);
	Write("Test 5: Managers get Clerks off their break when lines get too long\n", sizeof("Test 5: Managers get Clerks off their break when lines get too long\n"), ConsoleOutput);
	Write("Test 6: Total sales never suffers from a race condition\n", sizeof("Test 6: Total sales never suffers from a race condition\n"), ConsoleOutput);
	Write("Test 7: The behavior of Customers is proper when Senators arrive. This is before, during, and after.\n", sizeof("Test 7: The behavior of Customers is proper when Senators arrive. This is before, during, and after.\n"), ConsoleOutput);
	Write("Test 8: Full Simulation, user chooses number of threads\n", sizeof("Test 8: Full Simulation, user chooses number of threads\n"), ConsoleOutput);

	userChoice = ReadInt(1, 8);

	/*Set number of each thread based on user choice*/
	switch(userChoice){
		case 1:
		case 3:
			numCustomers = 5;
			numSenators = 0;
			numAppClerks = 0;
			numPicClerks = 0;
			numPassClerks = 0;
			numCashClerks = 2;
			break;
		case 2:
		case 6:
			numCustomers = 15;
			numSenators = 0;
			numAppClerks = 1;
			numPicClerks = 1;
			numPassClerks = 1;
			numCashClerks = 1;
			break;
		case 4:
			numCustomers = 3;
			numSenators = 0;
			numAppClerks = 0;
			numPicClerks = 0;
			numPassClerks = 0;
			numCashClerks = 5;
			break;
		case 5:
			numCustomers = 30;
			numSenators = 0;
			numAppClerks = 2;
			numPicClerks = 2;
			numPassClerks = 2;
			numCashClerks = 2;
			break;

		case 7:
			numCustomers = 15;
			numSenators = 2;
			numAppClerks = 2;
			numPicClerks = 2;
			numPassClerks = 2;
			numCashClerks = 2;
			break;

		case 8:

			/*Allow user to specify number of each thread*/
			Write("Enter number of customers.\n", sizeof("Enter number of customers.\n"), ConsoleOutput);
			numCustomers = ReadInt(0, 50);
			Write("Enter number of senators.\n", sizeof("Enter number of senators.\n"), ConsoleOutput);
			numSenators = ReadInt(0, 10); 
			Write("Enter number of ApplicationClerks.\n", sizeof("Enter number of ApplicationClerks.\n"), ConsoleOutput);
			numAppClerks = ReadInt(1, 5);
			Write("Enter number of PictureClerks.\n", sizeof("Enter number of PictureClerks.\n"), ConsoleOutput);
			numPicClerks = ReadInt(1, 5);
			Write("Enter number of PassportClerks.\n", sizeof("Enter number of PassportClerks.\n"), ConsoleOutput);
			numPassClerks = ReadInt(1, 5);
			Write("Enter number of Cashiers.\n", sizeof("Enter number of Cashiers.\n"), ConsoleOutput);
			numCashClerks = ReadInt(1, 5);
			break;
	}
	initializeVariables();
	StartSimulation();
	Exit(0);

}
