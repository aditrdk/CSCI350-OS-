#include "syscall.h"

int main() {
	int lockNum;
	lockNum = CreateLockRPC("Testlock", sizeof("Testlock"));
	PrintfInt("Lock Number %d\n", sizeof("Lock Number %d\n"), lockNum);
	ReleaseRPC(lockNum);
	PrintfInt("Released Lock Number %d\n", sizeof("Released Lock Number %d\n"), lockNum);
}