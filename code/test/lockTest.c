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

	Write("Destroying lock \n", sizeof("Destroying lock \n"), ConsoleOutput );

	DestroyLock(lockNum2);
	DestroyLock(-1);
	DestroyLock(55);
	DestroyLock(9000);

	Acquire(lockNum);
	Acquire(lockNum2);
	Acquire(33);
	Acquire(-22);


	Release(lockNum);
	Release(lockNum2);
	Release(-2);

	Acquire(lockNum);
	DestroyLock(lockNum);
	Acquire(lockNum);
	Release(lockNum);
	
	Acquire(lockNum);
	Release(lockNum);



	Halt();
}