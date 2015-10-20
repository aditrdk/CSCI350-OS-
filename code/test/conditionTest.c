#include "syscall.h"

int lockNum;
int lockNum2;
int conditionNum;
int conditionNum2;
int conditionNum3;
void waitThread() {
	Acquire(lockNum2);
	Write("wait thread Waiting\n", sizeof("wait thread Waiting\n"), ConsoleOutput );
	Wait(conditionNum2, lockNum2);
	Write("wait thread Woke up\n", sizeof("wait thread Woke up\n"), ConsoleOutput );
	Release(lockNum2);
	Exit(1);
}

void signalThread() {
	int i;
	int max;
	max = Rand()%6 + 10; 
	PrintfInt("Random number %d\n", sizeof("Random number %d\n"), max);
	max = Rand()%6 + 10; 
	PrintfInt("Random number %d\n", sizeof("Random number %d\n"), max);
	max = Rand()%6 + 10; 
	PrintfInt("Random number %d\n", sizeof("Random number %d\n"), max);
	for(i = 0; i < max; i++) {
		Yield();
	}
	Acquire(lockNum2);
	Write("Signal thread Signalling\n", sizeof("Signal thread Signalling\n"), ConsoleOutput );
	Signal(conditionNum2, lockNum2);
	Release(lockNum2);
	Exit(2);
}

void wrongLockThread() {
	Acquire(lockNum);
	Signal(conditionNum, lockNum);
	Release(lockNum);
	Exit(0);
}

int main() {
	lockNum = CreateLock("TestLock", sizeof("TestLock"));
	lockNum2 = CreateLock("TestLock_2", sizeof("TestLock_2"));

	conditionNum = CreateCondition("TestCondition", sizeof("TestCondition"));
	conditionNum2 = CreateCondition("TestCondition2", sizeof("TestCondition2"));
	conditionNum3 = CreateCondition("TestCondition3", sizeof("TestCondition3"));
	PrintfInt("condition 1 %d", sizeof("condition 1 %d"), conditionNum);
	PrintfInt("condition 2 %d", sizeof("condition 1 %d"), conditionNum2);
	PrintfInt("condition 3 %d", sizeof("condition 1 %d"), conditionNum3);
	DestroyCondition(12);
	Fork(waitThread);
	Fork(signalThread);
	Fork(wrongLockThread);
	Exit(0);
}