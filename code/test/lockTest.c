#include "syscall.h"

int main() {
	int lockNum;
	int lockNum2;
	Write("Testing CreateLock\n", sizeof("Testing CreateLock\n"), ConsoleOutput );

	lockNum = CreateLock("TestLock", sizeof("TestLock"));
	if(lockNum == 0) {
		Write("Lock index 0\n", sizeof("Lock index 0\n"), ConsoleOutput );
	}
	else {
		Write("Not index 0\n", sizeof("ock index 0\n"), ConsoleOutput );
	}

	lockNum2 = CreateLock("TestLock2", sizeof("TestLock2"));
	if(lockNum2 == 0) {
		Write("Lock index 0\n", sizeof("Lock index 0\n"), ConsoleOutput );
	}
	else {
		Write("Not index 0\n", sizeof("ock index 0\n"), ConsoleOutput );
	}
	Halt();
}