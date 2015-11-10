#include "syscall.h"

int main() {
	int lockNum, index;
	lockNum = CreateLockRPC("Testlock", sizeof("Testlock"));
	PrintfInt("Lock Number %d\n", sizeof("Lock Number %d\n"), lockNum);
	index = AcquireRPC(lockNum);
	PrintfInt("Acquired Lock Number %d\n", sizeof("Acquired Lock Number %d\n"), index);
	DestroyLockRPC(lockNum);
}