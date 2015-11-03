#include "syscall.h"

int lockNum;
int lockNum2;
int conditionNum;
int conditionNum2;
int conditionNum3;
void waitThread() {
	Acquire(lockNum2);
	Write("wait thread Waiting\n", sizeof("wait thread Waiting\n"), ConsoleOutput );
	/*Test condition wait*/
	Wait(conditionNum2, lockNum2);
	/*Printing this line means that someone signalled this thread*/
	Write("wait thread Woke up\n", sizeof("wait thread Woke up\n"), ConsoleOutput );
	Release(lockNum2);
	DestroyCondition(conditionNum2);
	DestroyLock(lockNum2);
	Exit(1);
}

void signalThread() {
	int i;
	int max;
	max = 5;
	/*Yielding so that wait thread runs first, if waitThread always runs first it shows that yield is working properly*/
	for(i = 0; i < max; i++) {
		Yield();
	}
	Acquire(lockNum2);
	Write("Signal thread Signalling\n", sizeof("Signal thread Signalling\n"), ConsoleOutput );
	/*Signalling a thread after acquiring the lock*/
	Signal(conditionNum2, lockNum2);
	Release(lockNum2);
	Exit(2);
}

void broadcastThread() {
	int i;
	int max;
	max = 30;
	/*Yielding so that wait thread runs first, if waitThread always runs first it shows that yield is working properly*/
	for(i = 0; i < max; i++) {
		Yield();
	}
	Acquire(lockNum);
	Write("broadcast thread broadcasting\n", sizeof("broadcast thread broadcasting\n"), ConsoleOutput );
	/*Broadcasting threads after acquiring the lock*/
	Broadcast(conditionNum, lockNum);
	Release(lockNum);
	Exit(2);
}

void sleepThread() {
	Acquire(lockNum);
	Write("sleep thread Waiting\n", sizeof("sleep thread Waiting\n"), ConsoleOutput );
	/*Test condition sleep*/
	Wait(conditionNum, lockNum);
	/*Printing this line means that someone signalled this thread*/
	Write("sleep thread Woke up\n", sizeof("sleep thread Woke up\n"), ConsoleOutput );
	Release(lockNum);
	Exit(1);
}

int main() {
	int i;
	lockNum = CreateLock("TestLock", sizeof("TestLock"));
	lockNum2 = CreateLock("TestLock_2", sizeof("TestLock_2"));
	/*Test create condition*/
	conditionNum = CreateCondition("TestCondition", sizeof("TestCondition"));
	conditionNum2 = CreateCondition("TestCondition2", sizeof("TestCondition2"));
	conditionNum3 = CreateCondition("TestCondition3", sizeof("TestCondition3"));
	PrintfInt("condition %d", sizeof("condition %d"), conditionNum);
	PrintfInt("condition %d", sizeof("condition %d"), conditionNum2);
	PrintfInt("condition %d", sizeof("condition %d"), conditionNum3);

	/*Signalling a thread without the lock*/
	Signal(conditionNum, lockNum);
	/*Wait on a condition without the lock*/
	Wait(conditionNum, lockNum);
	/*Broadcast a condition without the lock*/
	Broadcast(conditionNum, lockNum);

	Acquire(lockNum);
	/*Signal, wait and broadcast on a condition that hasn't been created*/
	Signal(15, lockNum);
	Wait(15, lockNum);
	Broadcast(15, lockNum);
	Release(lockNum);


	/*Destroying a condition that hasn't been created*/
	DestroyCondition(12);

	Fork(waitThread);
	Fork(signalThread);
	for(i = 0; i < 5; i++){
		Fork(sleepThread);
	}
	Fork(broadcastThread);


	Exit(0);
}