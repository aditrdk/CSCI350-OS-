#include "syscall.h"

int main() {
	int lockNum, conditionNum;
	lockNum = CreateLockRPC("ConditionLock", sizeof("ConditionLock"));
	PrintfInt("Lock Number %d\n", sizeof("Lock Number %d\n"), lockNum);
	conditionNum = CreateConditionRPC("TestCondition", sizeof("TestCondition"));
	PrintfInt("Condition Number %d\n", sizeof("Condition Number %d\n"), conditionNum);
	AcquireRPC(lockNum);
	SignalRPC(conditionNum, lockNum);
	ReleaseRPC(lockNum);
}