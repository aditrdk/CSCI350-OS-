#include "syscall.h"

int lockNum;
int conditionNum;

void waitThread() {
	Acquire(lockNum);
	Write("wait thread Waiting\n", sizeof("wait thread Waiting\n"), ConsoleOutput );
	Wait(conditionNum, lockNum);
	Write("wait thread Woke up\n", sizeof("wait thread Woke up\n"), ConsoleOutput );
	Release(lockNum);
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
	Acquire(lockNum);
	Write("Signal thread Signalling\n", sizeof("Signal thread Signalling\n"), ConsoleOutput );
	Signal(conditionNum, lockNum);
	Release(lockNum);
	Exit(2);
}

int main() {
	lockNum = CreateLock("TestLock", sizeof("TestLock"));
	conditionNum = CreateCondition("TestCondition", sizeof("TestCondition"));
	DestroyCondition(12);
	Fork(waitThread);
	Fork(signalThread);
	Exit(0);
}