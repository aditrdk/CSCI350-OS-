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

#include "copyright.h"
#include "system.h"
#include <stdlib.h>
#include <iostream.h>
#ifdef CHANGED
#include "synch.h"
#endif

enum CLERK_STATE {available, busy, onbreak};	


//Struct for customer's private data, the variables that are not accessible by other threads
struct Customer {
	//Customer keep track themselves of which clerks they ahve visted already successfully
	bool appAccepted; 
	bool isPicTaken;
	bool appFiled;
	bool hasPassport;
	bool hasPaidCashier;
	int cash;
	int ssn;

	Customer(int index) {
		ssn = index;
		appAccepted = false;
		isPicTaken = false;
		appFiled = false;
		hasPassport = false;
		hasPaidCashier = false;
		cash = (rand()%4)*500 + 100;
	}
};

//Customer data used by the Passport Office database that the clerks can look up via ssn
struct CustomerData {
	bool appAccepted;
	bool isPicTaken;
	bool appFiled;
	bool hasPassport;
	bool hasPaidCashier;
	int ssn;
	CustomerData() {
		appAccepted = false;
		isPicTaken = false;
		appFiled = false;
		hasPassport = false;
		hasPaidCashier = false;
	}
};

//Generic clerk data
struct Clerk {
	int numLine;			//number of people currently in line for this clerk
	int numBribeLine;
	bool servedOne;
	CLERK_STATE state;		//keep track of clerk state
	Condition* waitCondition;	//clerk uses this condition to signal next customer in line
	Condition* bribeWaitCondition;	//clerk uses this condition to signal next customer in line
	Lock* ClerkLock; 
	Condition* ClerkCV;
	int dataIn;				//Variable for passing data between clerk and customer
	Clerk() {
		numLine = 0;
		numBribeLine = 0;
		servedOne = false;
		state = busy;
	}
};


//Variable set based on the test being run
int numCustomers;
int numSenators;
int numAppClerks;
int numPicClerks;
int numPassClerks;
int numCashClerks;

//Test number
int userChoice;



//Data that can be changed by any thread. Each has their own associated lock to ensure mutal exclusion
int numCustomersWhoHaveLeftTheOffice = 0;
int numSenatorsWhoHaveLeftTheOffice = 0;
Lock numCustomersLock("Number_Customers_Lock");

Lock SenatorLock("Senator_Lock");
Condition SenatorWaitCondition("Senator_wait_Condition");
int numSenatorsPresent = 0; 
int numCustomersOutside = 0;

Lock appLineLock("App_Line_Lock");
Condition appBreakCondition("App_Break_Condition");
Clerk* appClerks;
int numAppClerksAwake;
int appClerkCash = 0; //Bribe line money Locked by line lock

Lock picLineLock("Pic_Line_Lock");
Condition picBreakCondition("Pic_Break_Condition");
Clerk* picClerks;
int numPicClerksAwake;
int picClerkCash = 0; //Bribe line money Locked by line lock

Lock passLineLock("Pass_Line_Lock");
Condition passBreakCondition("Pass_Break_Condition");
Clerk* passClerks;
int numPassClerksAwake;
int passClerkCash= 0; //Bribe line money Locked by line lock

Lock cashLineLock("Cash_Line_Lock");
Condition cashBreakCondition("cash_Break_Condition");
Clerk* cashClerks;
int numCashClerksAwake;
int cashClerkCash= 0; //Locked by line lock and special cashCashLock
Lock cashCashLock("Cash_Cash_Lock"); //extra lock for cashier cash variable because it is modified other than during the line bribing

//Passport office's internal database. 
CustomerData* customersDatabase;

#ifdef CHANGED
//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
	int num;

	for (num = 0; num < 5; num++) {
		printf("*** thread %d looped %d times\n", which, num);
		currentThread->Yield();
	}
}


//Function that customers go through at creation so they can be manipulated for better testing
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
		case 5:					//Causes most of the customers to enter later therefore clerks will go on break first and need to be woken up by the manager
			c->cash = 100;
			if(c->ssn > 6){
				for(int i = 0; i < 1000; i++) currentThread->Yield();
			}
			break;
	}

}


//Customer Main Thread
void CustomerThread(int index) {
	//Initialize new customer
	Customer* c = new Customer(index);
	c->ssn = index;
	initializeCustomerForTest(c);
	int clerkChoice = 0;

	//Loop until the customer has a passport
	while(!c->hasPassport){

		//Check for senators
		SenatorLock.Acquire();
		if(numSenatorsPresent != 0){
			printf("Customer %d is going outside the Passport Office because their is a Senator present.\n", index);
			numCustomersOutside++;
			while(numSenatorsPresent > 0) SenatorWaitCondition.Wait(&SenatorLock); //wait for senator to leave
			numCustomersOutside--;
		}else{

			//Choose new clerk type RANDOMLY
			clerkChoice = rand() % 4;
		}
		SenatorLock.Release();



		/////////// Choosing Application clerk
		if(!c->appAccepted && clerkChoice == 0){
			appLineLock.Acquire();	//check app clerk line
			int myLine = -1;
			int lineSize = 10000;		
			bool bribe = false;
			//Decide whether to bribe
			if(c->cash >= 500){
				bribe = true;
			}

			for(int i = 0; i < numAppClerks; i++) {		
				if (appClerks[i].state == available) {	//Check for available clerks
					myLine = i;
					lineSize = appClerks[i].numLine;
					bribe = false;
					break;
				}
				else if(!bribe && appClerks[i].numLine < lineSize && appClerks[i].state != onbreak) { //find shortest line
					myLine = i;
					lineSize = appClerks[i].numLine;
				}else if(bribe && appClerks[i].numBribeLine < lineSize && appClerks[i].state != onbreak) { //find shortest line
					myLine = i;
					lineSize = appClerks[i].numBribeLine;
				}
			}
			if(myLine == -1) { //No available lines
				printf("Customer %d says, \"Wow wtf, all app clerks on break?\"\n", index);
				appLineLock.Release();
				currentThread->Yield();
				continue;
			}else if(numSenatorsPresent != 0){
				//Check if they were taken out of line because of a senator
				appClerks[myLine].state = available;
				appLineLock.Release();
				continue;
			}
			else if(!bribe && appClerks[myLine].state == busy) { //Get in line
				appClerks[myLine].numLine++;
				printf("Customer %d has gotten in regular line for ApplicationClerk %d\n", index, myLine);
				appClerks[myLine].waitCondition->Wait(&appLineLock);
				appClerks[myLine].numLine--;
			}
			else if(bribe && appClerks[myLine].state == busy) { //Get in line
				appClerks[myLine].numBribeLine++;
				printf("Customer %d has gotten in bribe line for ApplicationClerk %d\n", index, myLine);
				printf("ApplicationClerk %d has received $500 from Customer %d\n", myLine, index);
				c->cash -= 500;
				appClerkCash += 500;
				appClerks[myLine].bribeWaitCondition->Wait(&appLineLock);
				appClerks[myLine].numBribeLine--;
			}
			else {
				appClerks[myLine].state = busy;			//If there is no one in line and the clerk is available they can go straight to the clerk
			}
			appLineLock.Release();
			//Check if they were taken out of line because of a senator
			if(numSenatorsPresent != 0){
				continue;
			}
			appClerks[myLine].ClerkLock->Acquire();
			appClerks[myLine].dataIn = c->ssn;
			printf("Customer %d has given SSN %d to ApplicationClerk %d\n", index, c->ssn, myLine);
			appClerks[myLine].ClerkCV->Signal(appClerks[myLine].ClerkLock); //Signal that ssn has been given
			appClerks[myLine].ClerkCV->Wait(appClerks[myLine].ClerkLock); //Waiting for clerk to take SSN and file application
			c->appAccepted = true;
			appClerks[myLine].ClerkCV->Signal(appClerks[myLine].ClerkLock);//Signal that customer is leaving counter
			appClerks[myLine].ClerkLock->Release();
		}
		
		/////////// Choosing Picture clerk
		if(!c->isPicTaken && clerkChoice == 1){
			//Going to Picture Clerk
			picLineLock.Acquire();
			int myLine = -1;
			int lineSize = 10000;	
			bool bribe = false;
			//Decide whether to bribe
			if(c->cash >= 500){
				bribe = true;
			}	
			for(int i = 0; i < numPicClerks; i++) {		
				if (picClerks[i].state == available) {	//Check for available clerks
					myLine = i;
					lineSize = picClerks[i].numLine;
					bribe = false; //do not bribe if clerk is available
					break;
				}
				else if(!bribe && picClerks[i].numLine < lineSize && picClerks[i].state != onbreak) { //find shortest line
					myLine = i;
					lineSize = picClerks[i].numLine;
				}
				else if(bribe && picClerks[i].numBribeLine < lineSize && picClerks[i].state != onbreak) { //find shortest line
					myLine = i;
					lineSize = picClerks[i].numBribeLine;
				}
			}
			if(myLine == -1) { //No available lines
				printf("Customer %d says, \"Wow wtf, all pic clerks on break?\"\n", index);
				picLineLock.Release();
				currentThread->Yield();
				continue;
			}else if(numSenatorsPresent != 0){
				//Check if they were taken out of line because of a senator
				picClerks[myLine].state = available;
				picLineLock.Release();
				continue;
			}
			else if(!bribe && picClerks[myLine].state == busy) { //Get in line
				picClerks[myLine].numLine++;
				printf("Customer %d has gotten in regular line for PictureClerk %d\n", index, myLine);
				picClerks[myLine].waitCondition->Wait(&picLineLock);
				picClerks[myLine].numLine--;
			}
			else if(bribe && picClerks[myLine].state == busy) { //Get in bribe line
				picClerks[myLine].numBribeLine++;
				printf("Customer %d has gotten in bribe line for PictureClerk %d\n", index, myLine);
				printf("PictureClerk %d has received $500 from Customer %d\n", myLine, index);
				c->cash -= 500;
				picClerkCash += 500;
				picClerks[myLine].bribeWaitCondition->Wait(&picLineLock);
				picClerks[myLine].numBribeLine--;
			}
			else {
				picClerks[myLine].state = busy;		//If there is no one in line and the clerk is available they can go straight to the clerk
			}
			picLineLock.Release();
			//Check if they were taken out of line because of a senator
			if(numSenatorsPresent != 0){
				continue;
			}
			picClerks[myLine].ClerkLock->Acquire();
			picClerks[myLine].dataIn = c->ssn;
			printf("Customer %d has given SSN %d to PictureClerk %d\n", index, c->ssn, myLine);
			picClerks[myLine].ClerkCV->Signal(picClerks[myLine].ClerkLock); //Signal that ssn has been given
			picClerks[myLine].ClerkCV->Wait(picClerks[myLine].ClerkLock); //Waiting for clerk to take SSN and take pic
			int likesPic = rand()%2;
			picClerks[myLine].dataIn = likesPic;
			if(likesPic == 0){ //Did not like pic
				printf("Customer %d does not like their picture from PictureClerk %d\n", index, myLine);
			}else{ //Likes Pic
				printf("Customer %d does like their picture from PictureClerk %d\n", index, myLine);
				c->isPicTaken = true;
			}
			picClerks[myLine].ClerkCV->Signal(picClerks[myLine].ClerkLock); //signalling whether he likes picture or not
			picClerks[myLine].ClerkCV->Wait(picClerks[myLine].ClerkLock); //Waiting for clerk to process pic or send customer away
			picClerks[myLine].ClerkLock->Release();
		}

		/////////// Choosing Passport clerk
		if(!c->appFiled && clerkChoice == 2) {
			passLineLock.Acquire();
			int myLine = -1;
			int lineSize = 10000;	
			bool bribe = false;
			//Decide whether to bribe
			if(c->cash >= 500){
				bribe = true;
			}	
			for(int i = 0; i < numPassClerks; i++) {		
				if (passClerks[i].state == available) {	//Check for available clerks
					myLine = i;
					lineSize = passClerks[i].numLine;
					bribe = false;
					break;
				}
				else if(!bribe && passClerks[i].numLine < lineSize && passClerks[i].state != onbreak) { //find shortest line
					myLine = i;
					lineSize = passClerks[i].numLine;
				}
				else if(bribe && passClerks[i].numBribeLine < lineSize && passClerks[i].state != onbreak) { //find shortest line
					myLine = i;
					lineSize = passClerks[i].numBribeLine;
				}
			}
			if(myLine == -1) { //No available lines
				printf("Customer %d says, \"Wow wtf, all pass clerks on break?\"\n", index);
				passLineLock.Release();
				currentThread->Yield();
				continue;
			}else if(numSenatorsPresent != 0){
				//Check if they were taken out of line because of a senator
				passClerks[myLine].state = available;
				passLineLock.Release();
				continue;
			}
			else if(!bribe && passClerks[myLine].state == busy) { //Get in line
				passClerks[myLine].numLine++;
				printf("Customer %d has gotten in regular line for PassportClerk %d at position %d\n", index, myLine, passClerks[myLine].numLine);
				passClerks[myLine].waitCondition->Wait(&passLineLock);
				passClerks[myLine].numLine--;
			}
			else if(bribe && passClerks[myLine].state == busy) { //Get in bribe line
				passClerks[myLine].numBribeLine++;
				printf("Customer %d has gotten in bribe line for PassportClerk %d at position %d\n", index, myLine, passClerks[myLine].numBribeLine);
				printf("PassportClerk %d has received $500 from Customer %d\n", myLine, index);
				c->cash -= 500;
				passClerkCash += 500;
				passClerks[myLine].bribeWaitCondition->Wait(&passLineLock);
				passClerks[myLine].numBribeLine--;
			}
			else {
				passClerks[myLine].state = busy;			//If there is no one in line and the clerk is available they can go straight to the clerk
			}
			passLineLock.Release();

			if(numSenatorsPresent != 0){
				continue;
			}
			passClerks[myLine].ClerkLock->Acquire();
			passClerks[myLine].dataIn = c->ssn;
			printf("Customer %d has given SSN %d to PassportClerk %d\n", index, c->ssn, myLine);
			passClerks[myLine].ClerkCV->Signal(passClerks[myLine].ClerkLock); //Signal that ssn has been given
			passClerks[myLine].ClerkCV->Wait(passClerks[myLine].ClerkLock);
			if(!(c->appAccepted && c->isPicTaken)) {
				printf("Customer %d has gone to PassportClerk %d too soon. They are going to the back of the line.\n", index, myLine);
				passClerks[myLine].ClerkLock->Release();
				int waitTime = rand()%901 + 100;
				for(int i = 0; i < waitTime; i++) {
					currentThread->Yield();
				}
			}
			else {
				c->appFiled = true;
				passClerks[myLine].ClerkLock->Release();
			}
		}

		/////////// Choosing Cashier
		if(!c->hasPassport && clerkChoice == 3) {
			cashLineLock.Acquire();
			int myLine = -1;
			int lineSize = 10000;	
			bool bribe = false;
			//Decide whether to bribe
			if(c->cash >= 500){
				bribe = true;
			}	
			for(int i = 0; i < numCashClerks; i++) {		
				if (cashClerks[i].state == available) {	//Check for available clerks
					myLine = i;
					lineSize = cashClerks[i].numLine;
					bribe = false;
					break;
				}
				else if(!bribe && cashClerks[i].numLine < lineSize && cashClerks[i].state != onbreak) { //find shortest line
					myLine = i;
					lineSize = cashClerks[i].numLine;
				}
				else if(bribe && cashClerks[i].numBribeLine < lineSize && cashClerks[i].state != onbreak) { //find shortest line
					myLine = i;
					lineSize = cashClerks[i].numBribeLine;
				}
			}
			if(myLine == -1) { //No available lines
				printf("Customer %d says, \"Wow wtf, all cashiers on break?\"\n", index);
				cashLineLock.Release();
				currentThread->Yield();
				continue;
			}else if(numSenatorsPresent != 0){
				cashClerks[myLine].state = available;
				cashLineLock.Release();
				continue;
			}
			else if(!bribe && cashClerks[myLine].state == busy) { //Get in line
				cashClerks[myLine].numLine++;
				printf("Customer %d has gotten in regular line for Cashier %d at position %d\n", index, myLine, cashClerks[myLine].numLine);
				cashClerks[myLine].waitCondition->Wait(&cashLineLock);
				cashClerks[myLine].numLine--;
			}
			else if(bribe && cashClerks[myLine].state == busy) { //Get in bribe line
				cashClerks[myLine].numBribeLine++;
				printf("Customer %d has gotten in bribe line for Cashier %d at position %d\n", index, myLine, cashClerks[myLine].numBribeLine);
				printf("Cashier %d has received $500 from Customer %d\n", myLine, index);
				c->cash -= 500;
				cashCashLock.Acquire();
				cashClerkCash += 500;
				cashCashLock.Release();
				cashClerks[myLine].bribeWaitCondition->Wait(&cashLineLock);
				cashClerks[myLine].numBribeLine--;
			}
			else {
				cashClerks[myLine].state = busy;		//If there is no one in line and the clerk is available they can go straight to the clerk
			}
			if(numSenatorsPresent != 0){
				continue;
			}
			cashLineLock.Release();
			//Check if they were taken out of line because of a senator
			cashClerks[myLine].ClerkLock->Acquire();
			cashClerks[myLine].dataIn = c->ssn;
			printf("Customer %d has given SSN %d to Cashier %d\n", index, c->ssn, myLine);
			if(!c->hasPaidCashier) {		//pay now if they havent paid yet
				c->cash -= 100;
				c->hasPaidCashier = true;
				printf("Customer %d has given Cashier %d 100$.\n", index, myLine);
			}
			cashClerks[myLine].ClerkCV->Signal(cashClerks[myLine].ClerkLock); //Signal that ssn and money has been given			
			cashClerks[myLine].ClerkCV->Wait(cashClerks[myLine].ClerkLock);
			if(!(c->appAccepted && c->isPicTaken && c->appFiled)) {
				printf("Customer %d has gone to Cashier %d too soon. They are going to the back of the line.\n", index, myLine);
				cashClerks[myLine].ClerkLock->Release();
				int waitTime = rand()%901 + 100;
				for(int i = 0; i < waitTime; i++) {
					currentThread->Yield();
				}
			}
			else {
				c->hasPassport = true;						//The customer has received their passport
				numCustomersLock.Acquire();
				numCustomersWhoHaveLeftTheOffice++;
				numCustomersLock.Release();
				printf("Customer %d is leaving the Passport Office.\n", index);			//They can now leave the office and end their thread
				cashClerks[myLine].ClerkLock->Release();
			}
		}
	}

}

void SenatorThread(int index){
	//Wait random time before entering office
	Customer* s = new Customer(index + numCustomers);
	s->ssn = index;
	int waitTime = rand() % 901 + 100;
	for(int i = 0; i < waitTime; i++) currentThread->Yield();
		SenatorLock.Acquire();
	numSenatorsPresent++;

	if(numSenatorsPresent > 1){ //not the first senator, must wait outside
		SenatorWaitCondition.Wait(&SenatorLock);
	}

	//Signals everyone to leave lines
	appLineLock.Acquire();
	for(int i = 0; i < numAppClerks; i++){
		if(appClerks[i].numLine > 0)appClerks[i].waitCondition->Broadcast(&appLineLock);
		if(appClerks[i].numBribeLine > 0)appClerks[i].bribeWaitCondition->Broadcast(&appLineLock);
	}
	appLineLock.Release();
	picLineLock.Acquire();
	for(int i = 0; i < numPicClerks; i++){
		if(picClerks[i].numLine > 0) picClerks[i].waitCondition->Broadcast(&picLineLock);
		if(picClerks[i].numBribeLine > 0) picClerks[i].bribeWaitCondition->Broadcast(&picLineLock);
	}
	picLineLock.Release();
	passLineLock.Acquire();
	for(int i = 0; i < numPassClerks; i++){
		if(passClerks[i].numLine > 0) passClerks[i].waitCondition->Broadcast(&passLineLock);
		if(passClerks[i].numBribeLine > 0)passClerks[i].bribeWaitCondition->Broadcast(&passLineLock);
	}
	passLineLock.Release();
	cashLineLock.Acquire();
	for(int i = 0; i < numCashClerks; i++){
		if(picClerks[i].numLine > 0) cashClerks[i].waitCondition->Broadcast(&cashLineLock);
		if(cashClerks[i].numBribeLine > 0) cashClerks[i].bribeWaitCondition->Broadcast(&cashLineLock);
	}
	cashLineLock.Release();

	//Actively checking until all customers have left
	while(numCustomersOutside < numCustomers - numCustomersWhoHaveLeftTheOffice) {
		SenatorLock.Release();
		currentThread->Yield();	
		SenatorLock.Acquire();
	}

	//Application Clerk
	appLineLock.Acquire();
	int myLine = -1;
	for(int i = 0; i < numAppClerks; i++) {
		if(appClerks[i].state == available) {
			myLine = i;
			printf("Senator %d has gotten in regular line for ApplicationClerk %d\n", index, myLine);
			break;
		}
	}
	appClerks[myLine].state = busy;
	appLineLock.Release();

	appClerks[myLine].ClerkLock->Acquire();
	appClerks[myLine].dataIn = s->ssn;
	printf("Senator %d has given SSN %d to ApplicationClerk %d\n", index, s->ssn, myLine);
	appClerks[myLine].ClerkCV->Signal(appClerks[myLine].ClerkLock); //Signal that ssn has been given
	appClerks[myLine].ClerkCV->Wait(appClerks[myLine].ClerkLock); //Waiting for clerk to take SSN and file application
	s->appAccepted = true;
	appClerks[myLine].ClerkCV->Signal(appClerks[myLine].ClerkLock);//Signal that customer is leaving counter
	appClerks[myLine].ClerkLock->Release();

	//Picture Clerk
	while(!s->isPicTaken) {
		picLineLock.Acquire();
		myLine = -1;
		for(int i = 0; i < numPicClerks; i++) {
			if(picClerks[i].state == available) {
				myLine = i;
				printf("Senator %d has gotten in regular line for PictureClerk %d\n", index, myLine);
				break;
			}
		}
		if(myLine == -1){
			picLineLock.Release();
			currentThread->Yield();
			continue;
		}
		picClerks[myLine].state = busy;
		picLineLock.Release();
		picClerks[myLine].ClerkLock->Acquire();
		picClerks[myLine].dataIn = s->ssn;
		printf("Senator %d has given SSN %d to PictureClerk %d\n", index, s->ssn, myLine);
		picClerks[myLine].ClerkCV->Signal(picClerks[myLine].ClerkLock); //Signal that ssn has been given
		picClerks[myLine].ClerkCV->Wait(picClerks[myLine].ClerkLock); //Waiting for clerk to take SSN and take pic
		int likesPic = rand()%2;
		picClerks[myLine].dataIn = likesPic;
		if(likesPic == 0){ //Did not like pic
			printf("Senator %d does not like their picture from PictureClerk %d\n", index, myLine);
		}else{ //Likes Pic
			printf("Senator %d does like their picture from PictureClerk %d\n", index, myLine);
			s->isPicTaken = true;
		}
		picClerks[myLine].ClerkCV->Signal(picClerks[myLine].ClerkLock); //signalling whether he likes picture or not
		picClerks[myLine].ClerkCV->Wait(picClerks[myLine].ClerkLock); //Waiting for clerk to process pic or send customer away
		picClerks[myLine].ClerkLock->Release();
	}

	//Passport clerk
	while(!s->appFiled) {
		passLineLock.Acquire();
		myLine = -1;
		for(int i = 0; i < numPassClerks; i++) {
			if(passClerks[i].state == available) {
				myLine = i;
				printf("Senator %d has gotten in regular line for PassportClerk %d\n", index, myLine);
				break;
			}
		}
		if(myLine == -1){
			passLineLock.Release();
			currentThread->Yield();
			continue;
		}
		passClerks[myLine].state = busy;
		passLineLock.Release();

		passClerks[myLine].ClerkLock->Acquire();
		passClerks[myLine].dataIn = s->ssn;
		printf("Senator %d has given SSN %d to PassportClerk %d\n", index, s->ssn, myLine);
		passClerks[myLine].ClerkCV->Signal(passClerks[myLine].ClerkLock); //Signal that ssn has been given
		passClerks[myLine].ClerkCV->Wait(passClerks[myLine].ClerkLock); //Waiting for clerk to take SSN and file application
		s->appFiled = true;
		passClerks[myLine].ClerkLock->Release();
	}

	//Cashier
	while(!s->hasPassport) {

		cashLineLock.Acquire();
		myLine = -1;
		for(int i = 0; i < numCashClerks; i++) {
			if(cashClerks[i].state == available) {
				myLine = i;
				printf("Senator %d has gotten in regular line for Cashier %d\n", index, myLine);
				break;
			}
		}
		if(myLine == -1){
			cashLineLock.Release();

			currentThread->Yield();
			continue;
		}
		cashClerks[myLine].state = busy;
		cashLineLock.Release();
		cashClerks[myLine].ClerkLock->Acquire();
		cashClerks[myLine].dataIn = s->ssn;
		printf("Senator %d has given SSN %d to Cashier %d\n", index, s->ssn, myLine);
		if(!s->hasPaidCashier) {			//pay now if they havent paid yet  
			s->cash -= 100;
			s->hasPaidCashier = true;
			printf("Senator %d has given Cashier %d 100$.\n", index, myLine);
		}
		cashClerks[myLine].ClerkCV->Signal(cashClerks[myLine].ClerkLock); //Signal that ssn and money has been given			
		cashClerks[myLine].ClerkCV->Wait(cashClerks[myLine].ClerkLock); // Wait to receive passport (they will always go in the right )
		if(!(s->appAccepted && s->isPicTaken && s->appFiled)) {
			printf("ERROR Senator %d has gone to cashier %d before other clerks, this shouldn't be possible.", index, myLine);
		}
		s->hasPassport = true;
		cashClerks[myLine].ClerkLock->Release();
	}
	numSenatorsPresent--;
	printf("Senator %d is leaving the Passport Office.\n", index);
	SenatorWaitCondition.Broadcast(&SenatorLock); //Let customers (and potentially other senator) know that he has left so they can enter
	SenatorLock.Release();
	numCustomersLock.Acquire();
	numSenatorsWhoHaveLeftTheOffice++;
	numCustomersLock.Release();
}

//Application Clerk thread
void AppClerkThread(int index) {
	Clerk* ac = &appClerks[index];
	char name[30];
	sprintf(name,"appClerk_%d_WaitCondition",index);
	ac->waitCondition = new Condition(name);
	sprintf(name,"appClerk_%d_BribeWaitCondition",index);
	ac->bribeWaitCondition = new Condition(name);
	sprintf(name,"appClerk_%d _ClerkLock",index);
	ac->ClerkLock = new Lock(name);
	sprintf(name,"appClerk_%d _ClerkCV",index);
	ac->ClerkCV = new Condition(name);	
	while(true){

		//End of simulation condition
		if(numCustomersWhoHaveLeftTheOffice + numSenatorsWhoHaveLeftTheOffice == numCustomers + numSenators) {
			break;
		}
		appLineLock.Acquire();
		if(ac->numBribeLine > 0){
			printf("ApplicationClerk %d has signalled a customer to come to their counter\n", index);
			ac->bribeWaitCondition->Signal(&appLineLock);
			ac->state = busy;		
		}else if(ac->numLine > 0){
			printf("ApplicationClerk %d has signalled a customer to come to their counter\n", index);
			ac->waitCondition->Signal(&appLineLock);
			ac->state = busy;
		}else if(numAppClerksAwake > 1 && ac->servedOne){				//If there is no one in line, there is atleast one other clerk of their type working and they have served a customer then they can go on break
			ac->state = onbreak;
			printf("ApplicationClerk %d is going on break\n", index);
			numAppClerksAwake--;
			appBreakCondition.Wait(&appLineLock);
			numAppClerksAwake++;
			printf("ApplicationClerk %d is coming off break\n", index);
			ac->state = available;
			ac->servedOne = false;
		}
		else{
			ac->state = available;							//If there is no one in line they set themselves to available
		}
		ac->ClerkLock->Acquire();
		appLineLock.Release();
		if(numSenatorsPresent != 0) {
			ac->state = available;
		}
		ac->ClerkCV->Wait(ac->ClerkLock); //Wait for customer to give ssn
		customersDatabase[ac->dataIn].ssn = ac->dataIn;
		printf("ApplicationClerk %d has received a SSN %d from Customer %d\n", index, ac->dataIn, ac->dataIn);
		int waitTime = rand() % 81 + 20;
		for(int i = 0; i < waitTime; i++) currentThread->Yield(); //Process for random amount of time(yields)
			customersDatabase[ac->dataIn].appAccepted = true;
		printf("ApplicationClerk %d has recorded a completed application for Customer %d\n", index, ac->dataIn);
		ac->ClerkCV->Signal(ac->ClerkLock); //Signal that application has been filed
		ac->ClerkCV->Wait(ac->ClerkLock); //Wait for customer to leave the counter
		ac->servedOne = true;
		ac->ClerkLock->Release();
	}
}

//Picture clerk thread
void PicClerkThread(int index){
	Clerk* pc = &picClerks[index];
	char name[30];
	sprintf(name,"picClerk_%d_WaitCondition",index);
	pc->waitCondition = new Condition(name);
	sprintf(name,"picClerk_%d_BribeWaitCondition",index);
	pc->bribeWaitCondition = new Condition(name);
	sprintf(name,"picClerk_%d _ClerkLock",index);
	pc->ClerkLock = new Lock(name);
	sprintf(name,"picClerk_%d _ClerkCV",index);
	pc->ClerkCV = new Condition(name);
	while(true){

		//End of simulation condition
		if(numCustomersWhoHaveLeftTheOffice + numSenatorsWhoHaveLeftTheOffice == numCustomers + numSenators) {
			break;
		}
		picLineLock.Acquire();
		if(pc->numBribeLine > 0){
			printf("PictureClerk %d has signalled a customer to come to their counter\n", index);
			pc->bribeWaitCondition->Signal(&picLineLock);
			pc->state = busy;		
		}else if(pc->numLine > 0){
			printf("PictureClerk %d has signalled a customer to come to their counter\n", index);
			pc->waitCondition->Signal(&picLineLock);
			pc->state = busy;
		}else if(numPicClerksAwake > 1 && pc->servedOne){				//If there is no one in line, there is atleast one other clerk of their type working and they have served a customer then they can go on break
			pc->state = onbreak;
			printf("PictureClerk %d is going on break\n", index);
			numPicClerksAwake--;
			picBreakCondition.Wait(&picLineLock);
			numPicClerksAwake++;
			printf("PictureClerk %d is coming off break\n", index);
			pc->state = available;
			pc->servedOne = false;
		}
		else{
			pc->state = available;			//If there is no one in line they set themselves to available
		}
		pc->ClerkLock->Acquire();
		picLineLock.Release();
		if(numSenatorsPresent != 0) {
			pc->state = available;
		}
		pc->ClerkCV->Wait(pc->ClerkLock); //Wait for customer to give ssn
		int customerID = pc->dataIn;
		customersDatabase[customerID].ssn = customerID;
		printf("PictureClerk %d has received a SSN %d from Customer %d\n", index, customerID, customerID);
		int waitTime = rand() % 41 + 10;
		for(int i = 0; i < waitTime; i++) currentThread->Yield(); //Take picture for random amount of time(yields)
		pc->ClerkCV->Signal(pc->ClerkLock); //Signal to customer picture was taken
		pc->ClerkCV->Wait(pc->ClerkLock); //Wait for customer to decide if they like it
		//if they did not like pic (and assumed to have left the counter)
		if(pc->dataIn == 0){
			printf("PictureClerk %d has been told that Customer %d does not like their picture\n", index, customerID);

		}//if they liked pic
		else{
			printf("PictureClerk %d has been told that Customer %d does like their picture\n", index, customerID);
			waitTime = rand() % 81 + 20;
			for(int i = 0; i < waitTime; i++) currentThread->Yield(); //Process picture for random amount of time(yields)
				customersDatabase[customerID].isPicTaken = true;
		}
		pc->ClerkCV->Signal(pc->ClerkLock); //Signal to customer picture was processed or that the customer can get back in line
		pc->servedOne = true;

		pc->ClerkLock->Release();
	}
}

//Passport Clerk Thread
void PassClerkThread(int index) {
	Clerk* ptc = &passClerks[index];
	char name[40];
	sprintf(name,"passClerk_%d_WaitCondition",index);
	ptc->waitCondition = new Condition(name);
	sprintf(name,"passClerk_%d_BribeWaitCondition",index);
	ptc->bribeWaitCondition = new Condition(name);
	sprintf(name,"passClerk_%d _ClerkLock",index);
	ptc->ClerkLock = new Lock(name);
	sprintf(name,"passClerk_%d _ClerkCV",index);
	ptc->ClerkCV = new Condition(name);
	int waitTime;
	while(true) {
		
		//End of simulation condition
		if(numCustomersWhoHaveLeftTheOffice + numSenatorsWhoHaveLeftTheOffice == numCustomers + numSenators) {
			break;
		}
		passLineLock.Acquire();
		if(ptc->numBribeLine > 0){
			printf("PassportClerk %d has signalled a customer to come to their counter\n", index);
			ptc->bribeWaitCondition->Signal(&passLineLock);
			ptc->state = busy;		
		}else if(ptc->numLine > 0){
			printf("PassportClerk %d has signalled a customer to come to their counter\n", index);
			ptc->waitCondition->Signal(&passLineLock);
			ptc->state = busy;
		}else if(numPassClerksAwake > 1 && ptc->servedOne){							//If there is no one in line, there is atleast one other clerk of their type working and they have served a customer then they can go on break
			ptc->state = onbreak;
			printf("PassportClerk %d is going on break\n", index);
			numPassClerksAwake--;
			passBreakCondition.Wait(&passLineLock);
			numPassClerksAwake++;
			printf("PassportClerk %d is coming off break\n", index);
			ptc->state = available;
			ptc->servedOne = false;
		}
		else{
			ptc->state = available;							//If there is no one in line they set themselves to available
		}
		ptc->ClerkLock->Acquire();
		passLineLock.Release();
		if(numSenatorsPresent != 0) {
			ptc->state = available;
		}
		ptc->ClerkCV->Wait(ptc->ClerkLock); //Wait for customer to give ssn
		int customerID = ptc->dataIn;
		printf("PassportClerk %d has received a SSN %d from Customer %d\n", index, customerID, customerID);
		if(!(customersDatabase[customerID].isPicTaken && customersDatabase[customerID].appAccepted)) {
			printf("PassportClerk %d has determined that Customer %d does not have both their application and picture completed\n", index, customerID);
		} 
		else {
			printf("PassportClerk %d has determined that Customer %d has both their application and picture completed\n", index, customerID);
			waitTime = rand() % 81 + 20;
			for(int i = 0; i < waitTime; i++) currentThread->Yield(); //Recording customer documents
				customersDatabase[customerID].appFiled = true;
			printf("PassportClerk %d has has recorded Customer %d passport documentation\n", index, customerID);
		}
		ptc->ClerkCV->Signal(ptc->ClerkLock);
		ptc->servedOne = true;
		ptc->ClerkLock->Release();
	}
}

//Cashier Thread
void CashClerkThread(int index) {
	Clerk* cc = &cashClerks[index];
	char name[40];
	sprintf(name,"cashClerk_%d_WaitCondition",index);
	cc->waitCondition = new Condition(name);
	sprintf(name,"cashClerk_%d_BribeWaitCondition",index);
	cc->bribeWaitCondition = new Condition(name);
	sprintf(name,"cashClerk_%d _ClerkLock",index);
	cc->ClerkLock = new Lock(name);
	sprintf(name,"cashClerk_%d _ClerkCV",index);
	cc->ClerkCV = new Condition(name);
	int waitTime;
	while(true) {
		
		//End of simulation condition
		if(numCustomersWhoHaveLeftTheOffice + numSenatorsWhoHaveLeftTheOffice == numCustomers + numSenators) {	//If all customers have got their passports then end the thread
			break;
		}
		cashLineLock.Acquire();
		if(cc->numBribeLine > 0){
			printf("Cashier %d has signalled a customer to come to their counter\n", index);
			cc->bribeWaitCondition->Signal(&cashLineLock);
			cc->state = busy;		
		}else if(cc->numLine > 0){
			printf("Cashier %d has signalled a customer to come to their counter\n", index);
			cc->waitCondition->Signal(&cashLineLock);
			cc->state = busy;
		}else if(numCashClerksAwake > 1 && cc->servedOne){							//If there is no one in line, there is atleast one other clerk of their type working and they have served a customer then they can go on break
			cc->state = onbreak;
			printf("Cashier %d is going on break\n", index);
			numCashClerksAwake--;
			cashBreakCondition.Wait(&cashLineLock);
			numCashClerksAwake++;
			printf("Cashier %d is coming off break\n", index);
			cc->state = available;
			cc->servedOne = false;
		}
		else{
			cc->state = available;				//If there is no one in line they set themselves to available
		}

		cc->ClerkLock->Acquire();
		cashLineLock.Release();

		if(numSenatorsPresent != 0) {
			cc->state = available;
		}

		cc->ClerkCV->Wait(cc->ClerkLock); //Wait for customer to give ssn
		int customerID = cc->dataIn;
		printf("Cashier %d has received a SSN %d from Customer %d\n", index, customerID, customerID);
		if((customersDatabase[customerID].isPicTaken && customersDatabase[customerID].appAccepted && customersDatabase[customerID].appFiled)) {		//checking if the customer has been certified yet
			printf("Cashier %d has verified that Customer %d has been certified by a PassportClerk\n", index, customerID);	
			if(!customersDatabase[customerID].hasPaidCashier) {
				printf("Cashier %d has received the $100 from Customer %d after certification.\n", index, customerID);
				cashCashLock.Acquire();
				cashClerkCash += 100;
				cashCashLock.Release();
				customersDatabase[customerID].hasPaidCashier = true;
			}
			waitTime = rand() % 81 + 20;
			for(int i = 0; i < waitTime; i++) currentThread->Yield(); //Recording customer documents
				printf("Cashier %d has provided Customer %d their completed passport\n", index, customerID);
			customersDatabase[customerID].hasPassport = true;
		}
		else if(!customersDatabase[customerID].hasPaidCashier) {
			printf("Cashier %d has received the $100 from Customer %d before certification. They are to go to the back of my line.\n", index, customerID);			
			cashCashLock.Acquire();
			cashClerkCash += 100;				//Add the money to the clerk total
			cashCashLock.Release();
			customersDatabase[customerID].hasPaidCashier = true;
		}
		cc->ClerkCV->Signal(cc->ClerkLock);
		if(customersDatabase[customerID].hasPassport){
			printf("Cashier %d has recorded that Customer %d has been given their completed passport\n", index, customerID);
		}
		cc->servedOne = true;
		cc->ClerkLock->Release();
	}
}

void ManagerThread(int index) {
	int iterations = 0, total = 0, printCount = 10;
	while(true) {
		total = 0;
		appLineLock.Acquire();
		for(int i = 0; i < numAppClerks; i++) {
			if(appClerks[i].numLine >= 3 || appClerks[i].numBribeLine >= 3 || numAppClerksAwake == 0) {		//check to see if there are more than 3 people in any of the lines
				for(int in = 0; in < numAppClerks; in++) {	
					if(appClerks[in].state == onbreak) {				//if the clerk is on break wake them up				
						appBreakCondition.Signal(&appLineLock);
						printf("Manager has woken up an ApplicationClerk\n");
						break;
					}
				}
				break;
			}
		}
		total += appClerkCash;
		if(iterations >= printCount) printf("Manager has counted a total of $%d for ApplicationClerks\n", appClerkCash);
		appLineLock.Release();
		picLineLock.Acquire();
		for(int i = 0; i < numPicClerks; i++) {
			if(picClerks[i].numLine >= 3 || picClerks[i].numBribeLine >= 3 || numPicClerksAwake == 0) {		//check to see if there are more than 3 people in any of the lines
				for(int in = 0; in < numPicClerks; in++) {
					if(picClerks[in].state == onbreak) {				//if the clerk is on break wake them up		
						picBreakCondition.Signal(&picLineLock);
						printf("Manager has woken up a PictureClerk\n");
						break;
					}
				}
				break;
			}
		}
		total += picClerkCash;
		if(iterations >= printCount) printf("Manager has counted a total of $%d for PictureClerks\n", picClerkCash);
		picLineLock.Release();
		passLineLock.Acquire();
		for(int i = 0; i < numPassClerks; i++) {

			if(passClerks[i].numLine >= 3 || passClerks[i].numBribeLine >= 3 || numPassClerksAwake == 0) {		//check to see if there are more than 3 people in any of the lines
				for(int in = 0; in < numPassClerks; in++) {
					if(passClerks[in].state == onbreak) {				//if the clerk is on break wake them up		
						passBreakCondition.Signal(&passLineLock);
						printf("Manager has woken up a PassportClerk\n");
						break;
					}
				}
				break;
			}
		}
		total += passClerkCash;
		if(iterations >= printCount) printf("Manager has counted a total of $%d for PassportClerks\n", passClerkCash);
		passLineLock.Release();
		cashLineLock.Acquire();
		for(int i = 0; i < numCashClerks; i++) {
			if(cashClerks[i].numLine >= 3 || cashClerks[i].numBribeLine >= 3 || numCashClerksAwake == 0) {		//check to see if there are more than 3 people in any of the lines
				for(int in = 0; in < numCashClerks; in++) {
					if(cashClerks[in].state == onbreak) {				//if the clerk is on break wake them up		
						cashBreakCondition.Signal(&cashLineLock);
						printf("Manager has woken up a Cashier\n");
						break;
					}
				}
				break;
			}
		}
		cashCashLock.Acquire();
		total += cashClerkCash;
		if(iterations >= printCount) printf("Manager has counted a total of $%d for Cashiers\n", cashClerkCash);		//lock when calculating cash to avoid race conditions
		cashCashLock.Release();
		cashLineLock.Release();
		for(int i = 0; i < 35; i++) {
			currentThread->Yield();
		}
		if(iterations >= printCount) {
			printf("Manager has counted a total of $%d for the passport office\n", total);		//printing total money made at the passport office
			iterations = 0;
		}
		
		//End of simulation condition
		if(numCustomersWhoHaveLeftTheOffice + numSenatorsWhoHaveLeftTheOffice == numCustomers + numSenators) {
			break;
		}
		iterations++;
	}
}

//----------------------------------------------------------------------
// ThreadTest
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest()
{
	DEBUG('t', "Entering SimpleTest");

	Thread *t = new Thread("forked thread");

	t->Fork(SimpleThread, 1);
	SimpleThread(0);
}

//	Simple test cases for the threads assignment.
//

// --------------------------------------------------
// Test Suite
// --------------------------------------------------


// --------------------------------------------------
// Test 1 - see TestSuite() for details
// --------------------------------------------------
Semaphore t1_s1("t1_s1",0);       // To make sure t1_t1 acquires the
                                  // lock before t1_t2
Semaphore t1_s2("t1_s2",0);       // To make sure t1_t2 Is waiting on the 
                                  // lock before t1_t3 releases it
Semaphore t1_s3("t1_s3",0);       // To make sure t1_t1 does not release the
                                  // lock before t1_t3 tries to acquire it
Semaphore t1_done("t1_done",0);   // So that TestSuite knows when Test 1 is
                                  // done
Lock t1_l1("t1_l1");		  // the lock tested in Test 1

// --------------------------------------------------
// t1_t1() -- test1 thread 1
//     This is the rightful lock owner
// --------------------------------------------------
void t1_t1() {
	t1_l1.Acquire();
    t1_s1.V();  // Allow t1_t2 to try to Acquire Lock

    printf ("%s: Acquired Lock %s, waiting for t3\n",currentThread->getName(),
    	t1_l1.getName());
    t1_s3.P();
    printf ("%s: working in CS\n",currentThread->getName());
    for (int i = 0; i < 1000000; i++) ;
    	printf ("%s: Releasing Lock %s\n",currentThread->getName(),
    		t1_l1.getName());
    t1_l1.Release();
    t1_done.V();
}

// --------------------------------------------------
// t1_t2() -- test1 thread 2
//     This thread will wait on the held lock.
// --------------------------------------------------
void t1_t2() {

    t1_s1.P();	// Wait until t1 has the lock
    t1_s2.V();  // Let t3 try to acquire the lock

    printf("%s: trying to acquire lock %s\n",currentThread->getName(),
    	t1_l1.getName());
    t1_l1.Acquire();

    printf ("%s: Acquired Lock %s, working in CS\n",currentThread->getName(),
    	t1_l1.getName());
    for (int i = 0; i < 10; i++)
    	;
    printf ("%s: Releasing Lock %s\n",currentThread->getName(),
    	t1_l1.getName());
    t1_l1.Release();
    t1_done.V();
}

// --------------------------------------------------
// t1_t3() -- test1 thread 3
//     This thread will try to release the lock illegally
// --------------------------------------------------
void t1_t3() {

    t1_s2.P();	// Wait until t2 is ready to try to acquire the lock

    t1_s3.V();	// Let t1 do it's stuff
    for ( int i = 0; i < 3; i++ ) {
    	printf("%s: Trying to release Lock %s\n",currentThread->getName(),
    		t1_l1.getName());
    	t1_l1.Release();
    }
}

// --------------------------------------------------
// Test 2 - see TestSuite() for details
// --------------------------------------------------
Lock t2_l1("t2_l1");		// For mutual exclusion
Condition t2_c1("t2_c1");	// The condition variable to test
Semaphore t2_s1("t2_s1",0);	// To ensure the Signal comes before the wait
Semaphore t2_done("t2_done",0);     // So that TestSuite knows when Test 2 is
                                  // done

// --------------------------------------------------
// t2_t1() -- test 2 thread 1
//     This thread will signal a variable with nothing waiting
// --------------------------------------------------
void t2_t1() {
	t2_l1.Acquire();
	printf("%s: Lock %s acquired, signalling %s\n",currentThread->getName(),
		t2_l1.getName(), t2_c1.getName());
	t2_c1.Signal(&t2_l1);
	printf("%s: Releasing Lock %s\n",currentThread->getName(),
		t2_l1.getName());
	t2_l1.Release();
    t2_s1.V();	// release t2_t2
    t2_done.V();
}

// --------------------------------------------------
// t2_t2() -- test 2 thread 2
//     This thread will wait on a pre-signalled variable
// --------------------------------------------------
void t2_t2() {
    t2_s1.P();	// Wait for t2_t1 to be done with the lock
    t2_l1.Acquire();
    printf("%s: Lock %s acquired, waiting on %s\n",currentThread->getName(),
    	t2_l1.getName(), t2_c1.getName());
    t2_c1.Wait(&t2_l1);
    printf("%s: Releasing Lock %s\n",currentThread->getName(),
    	t2_l1.getName());
    t2_l1.Release();
}
// --------------------------------------------------
// Test 3 - see TestSuite() for details
// --------------------------------------------------
Lock t3_l1("t3_l1");		// For mutual exclusion
Condition t3_c1("t3_c1");	// The condition variable to test
Semaphore t3_s1("t3_s1",0);	// To ensure the Signal comes before the wait
Semaphore t3_done("t3_done",0); // So that TestSuite knows when Test 3 is
                                // done

// --------------------------------------------------
// t3_waiter()
//     These threads will wait on the t3_c1 condition variable.  Only
//     one t3_waiter will be released
// --------------------------------------------------
void t3_waiter() {
	t3_l1.Acquire();
    t3_s1.V();		// Let the signaller know we're ready to wait
    printf("%s: Lock %s acquired, waiting on %s\n",currentThread->getName(),
    	t3_l1.getName(), t3_c1.getName());
    t3_c1.Wait(&t3_l1);
    printf("%s: freed from %s\n",currentThread->getName(), t3_c1.getName());
    t3_l1.Release();
    t3_done.V();
}


// --------------------------------------------------
// t3_signaller()
//     This threads will signal the t3_c1 condition variable.  Only
//     one t3_signaller will be released
// --------------------------------------------------
void t3_signaller() {

    // Don't signal until someone's waiting

	for ( int i = 0; i < 5 ; i++ ) 
		t3_s1.P();
	t3_l1.Acquire();
	printf("%s: Lock %s acquired, signalling %s\n",currentThread->getName(),
		t3_l1.getName(), t3_c1.getName());
	t3_c1.Signal(&t3_l1);
	printf("%s: Releasing %s\n",currentThread->getName(), t3_l1.getName());
	t3_l1.Release();
	t3_done.V();
}

// --------------------------------------------------
// Test 4 - see TestSuite() for details
// --------------------------------------------------
Lock t4_l1("t4_l1");		// For mutual exclusion
Condition t4_c1("t4_c1");	// The condition variable to test
Semaphore t4_s1("t4_s1",0);	// To ensure the Signal comes before the wait
Semaphore t4_done("t4_done",0); // So that TestSuite knows when Test 4 is
                                // done

// --------------------------------------------------
// t4_waiter()
//     These threads will wait on the t4_c1 condition variable.  All
//     t4_waiters will be released
// --------------------------------------------------
void t4_waiter() {
	t4_l1.Acquire();
    t4_s1.V();		// Let the signaller know we're ready to wait
    printf("%s: Lock %s acquired, waiting on %s\n",currentThread->getName(),
    	t4_l1.getName(), t4_c1.getName());
    t4_c1.Wait(&t4_l1);
    printf("%s: freed from %s\n",currentThread->getName(), t4_c1.getName());
    t4_l1.Release();
    t4_done.V();
}


// --------------------------------------------------
// t2_signaller()
//     This thread will broadcast to the t4_c1 condition variable.
//     All t4_waiters will be released
// --------------------------------------------------
void t4_signaller() {

    // Don't broadcast until someone's waiting

	for ( int i = 0; i < 5 ; i++ ) 
		t4_s1.P();
	t4_l1.Acquire();
	printf("%s: Lock %s acquired, broadcasting %s\n",currentThread->getName(),
		t4_l1.getName(), t4_c1.getName());
	t4_c1.Broadcast(&t4_l1);
	printf("%s: Releasing %s\n",currentThread->getName(), t4_l1.getName());
	t4_l1.Release();
	t4_done.V();
}
// --------------------------------------------------
// Test 5 - see TestSuite() for details
// --------------------------------------------------
Lock t5_l1("t5_l1");		// For mutual exclusion
Lock t5_l2("t5_l2");		// Second lock for the bad behavior
Condition t5_c1("t5_c1");	// The condition variable to test
Semaphore t5_s1("t5_s1",0);	// To make sure t5_t2 acquires the lock after
                                // t5_t1

// --------------------------------------------------
// t5_t1() -- test 5 thread 1
//     This thread will wait on a condition under t5_l1
// --------------------------------------------------
void t5_t1() {
	t5_l1.Acquire();
    t5_s1.V();	// release t5_t2
    printf("%s: Lock %s acquired, waiting on %s\n",currentThread->getName(),
    	t5_l1.getName(), t5_c1.getName());
    t5_c1.Wait(&t5_l1);
    printf("%s: Releasing Lock %s\n",currentThread->getName(),
    	t5_l1.getName());
    t5_l1.Release();
}

// --------------------------------------------------
// t5_t1() -- test 5 thread 1
//     This thread will wait on a t5_c1 condition under t5_l2, which is
//     a Fatal error
// --------------------------------------------------
void t5_t2() {
    t5_s1.P();	// Wait for t5_t1 to get into the monitor
    t5_l1.Acquire();
    t5_l2.Acquire();
    printf("%s: Lock %s acquired, signalling %s\n",currentThread->getName(),
    	t5_l2.getName(), t5_c1.getName());
    t5_c1.Signal(&t5_l2);
    printf("%s: Releasing Lock %s\n",currentThread->getName(),
    	t5_l2.getName());
    t5_l2.Release();
    printf("%s: Releasing Lock %s\n",currentThread->getName(),
    	t5_l1.getName());
    t5_l1.Release();
}

// --------------------------------------------------
// TestSuite()
//     This is the main thread of the test suite.  It runs the
//     following tests:
//
//       1.  Show that a thread trying to release a lock it does not
//       hold does not work
//
//       2.  Show that Signals are not stored -- a Signal with no
//       thread waiting is ignored
//
//       3.  Show that Signal only wakes 1 thread
//
//	 4.  Show that Broadcast wakes all waiting threads
//
//       5.  Show that Signalling a thread waiting under one lock
//       while holding another is a Fatal error
//
//     Fatal errors terminate the thread in question.
// --------------------------------------------------
void TestSuite() {
	Thread *t;
	char *name;
	int i;

    // Test 1

	printf("Starting Test 1\n");

	t = new Thread("t1_t1");
	t->Fork((VoidFunctionPtr)t1_t1,0);

	t = new Thread("t1_t2");
	t->Fork((VoidFunctionPtr)t1_t2,0);

	t = new Thread("t1_t3");
	t->Fork((VoidFunctionPtr)t1_t3,0);

    // Wait for Test 1 to complete
	for (  i = 0; i < 2; i++ )
		t1_done.P();

    // Test 2

	printf("Starting Test 2.  Note that it is an error if thread t2_t2\n");
	printf("completes\n");

	t = new Thread("t2_t1");
	t->Fork((VoidFunctionPtr)t2_t1,0);

	t = new Thread("t2_t2");
	t->Fork((VoidFunctionPtr)t2_t2,0);

    // Wait for Test 2 to complete
	t2_done.P();

    // Test 3

	printf("Starting Test 3\n");

	for (  i = 0 ; i < 5 ; i++ ) {
		name = new char [20];
		sprintf(name,"t3_waiter%d",i);
		t = new Thread(name);
		t->Fork((VoidFunctionPtr)t3_waiter,0);
	}
	t = new Thread("t3_signaller");
	t->Fork((VoidFunctionPtr)t3_signaller,0);

    // Wait for Test 3 to complete
	for (  i = 0; i < 2; i++ )
		t3_done.P();

    // Test 4

	printf("Starting Test 4\n");

	for (  i = 0 ; i < 5 ; i++ ) {
		name = new char [20];
		sprintf(name,"t4_waiter%d",i);
		t = new Thread(name);
		t->Fork((VoidFunctionPtr)t4_waiter,0);
	}
	t = new Thread("t4_signaller");
	t->Fork((VoidFunctionPtr)t4_signaller,0);

    // Wait for Test 4 to complete
	for (  i = 0; i < 6; i++ )
		t4_done.P();

    // Test 5

	printf("Starting Test 5.  Note that it is an error if thread t5_t1\n");
	printf("completes\n");

	t = new Thread("t5_t1");
	t->Fork((VoidFunctionPtr)t5_t1,0);

	t = new Thread("t5_t2");
	t->Fork((VoidFunctionPtr)t5_t2,0);

}


void Test1(){


}

void StartSimulation(){
	char * name;
	numAppClerksAwake = numAppClerks;
	numPicClerksAwake = numPicClerks;
	numPassClerksAwake = numPassClerks;
	numCashClerksAwake = numCashClerks;
	customersDatabase = new CustomerData[numCustomers + numSenators];
	appClerks = new Clerk[numAppClerks]();
	picClerks = new Clerk[numPicClerks]();
	passClerks = new Clerk[numPassClerks]();
	cashClerks = new Clerk[numCashClerks]();

	for( int i = 0; i < numAppClerks; i++) {
		name = new char [20];
		sprintf(name,"appClerk%d",i);
		Thread * t = new Thread(name);
		t->Fork(AppClerkThread, i);
	}

	for( int i = 0; i < numPicClerks; i++) {
		name = new char [20];
		sprintf(name,"picClerk%d",i);
		Thread * t = new Thread(name);
		t->Fork(PicClerkThread, i);
	}

	for( int i = 0; i < numPassClerks; i++) {
		name = new char [20];
		sprintf(name,"passClerk%d",i);
		Thread * t = new Thread(name);
		t->Fork(PassClerkThread, i);
	}

	for( int i = 0; i < numCashClerks; i++) {
		name = new char [20];
		sprintf(name,"cashClerk%d",i);
		Thread * t = new Thread(name);
		t->Fork(CashClerkThread, i);
	}

	for( int i = 0; i < numCustomers; i++) {
		name = new char [20];
		sprintf(name,"customer%d",i);
		Thread * t = new Thread(name);
		t->Fork(CustomerThread, i);
	}

	Thread * mt = new Thread("Manager");
	mt->Fork(ManagerThread, 0);

	for( int i = 0; i < numSenators; i++) {
		name = new char [20];
		sprintf(name,"senator%d",i);
		Thread * t = new Thread(name);
		t->Fork(SenatorThread, (i + numCustomers));
	}

	delete name;
}

//Part 2 of the project gives user choice of test to run
void Problem2() {
	srand(420);
	printf("%s\n", "Enter the number of the test you would like to run");
	printf("%s\n", "Test 1: Customers always take the shortest line, but no 2 customers ever choose the same shortest line at the same time");
	printf("%s\n", "Test 2: Managers only read one from one Clerk's total money received, at a time.");
	printf("%s\n", "Test 3: Customers do not leave until they are given their passport by the Cashier. The Cashier does not start on another customer until they know that the last Customer has left their area");
	printf("%s\n", "Test 4: Clerks go on break when they have no one waiting in their line");
	printf("%s\n", "Test 5: Managers get Clerks off their break when lines get too long");
	printf("%s\n", "Test 6: Total sales never suffers from a race condition");
	printf("%s\n", "Test 7: The behavior of Customers is proper when Senators arrive. This is before, during, and after.");
	printf("%s\n", "Test 8: Full Simulation, user chooses number of threads");

	std::cin>>userChoice;
	if(std::cin.fail()){
		std::cin.clear();
    	std::cin.ignore(80, '\n');
    	userChoice = -1;
	}
	while(!(userChoice < 8 && userChoice > 1)){
		printf("%s\n", "Not a valid test");
		if(std::cin.fail()){
			std::cin.clear();
        	std::cin.ignore(80, '\n');
		}
		std::cin>>userChoice;
	}

	//Set number of each thread based on user choice
	switch(userChoice){
		case 1:
		case 3:
			numCustomers = 
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

			//Allow user to specify number of each thread
			printf("Enter number of customers.\n");
			std::cin>>numCustomers;
			printf("Enter number of senators.\n");
			std::cin>>numSenators; 
			printf("Enter number of ApplicationClerks.\n");
			std::cin>>numAppClerks;
			printf("Enter number of PictureClerks.\n");
			std::cin>>numPicClerks;
			printf("Enter number of PassportClerks.\n");
			std::cin>>numPassClerks;
			printf("Enter number of Cashiers.\n");
			std::cin>>numCashClerks;
			break;
	}
	StartSimulation();


}

#endif
