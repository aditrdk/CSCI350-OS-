#include "syscall.h"

int lockNum2;

void lockAcquireThread() {
	/*Release a lock you don't own*/
	Release(lockNum2);
	/*Acquire a lock that's currently being held*/
	PrintfInt("lock Acquire Thread trying to acquire lock num %d\n", sizeof("lock Acquire Thread trying to acquire lock num %d\n"), lockNum2);
	Acquire(lockNum2);
	PrintfInt("lock Acquire Thread acquired lock num %d\n", sizeof("lock Acquire Thread acquired lock num %d\n"), lockNum2);
	/*Try to destroy a lock that's in use*/
	DestroyLock(lockNum2);
	/*Release a lock that is marked to be deleted*/
	Release(lockNum2);
	Exit(1);
}

int main() {
	int lockNum;
	int i;
	Write("Testing CreateLock\n", sizeof("Testing CreateLock\n"), ConsoleOutput );
	/* Test create lock*/
	lockNum = CreateLock("TestLock", sizeof("TestLock"));
	lockNum2 = CreateLock("TestLock2", sizeof("TestLock2"));

	Yield();

	/*Test Acquire Lock*/
	Acquire(lockNum);	
	Acquire(lockNum);
	Acquire(lockNum2);
	Fork(lockAcquireThread);
	Acquire(-22);
	Acquire(33);
	for(i = 0; i < 5; i++){ 
		Yield();
	}

	/*Test Release Lock*/
	PrintfInt("main Thread releasing lock num %d\n", sizeof("main Thread releasing lock num %d\n"), lockNum2);
	Release(lockNum2);
	Release(-2);

	/*Test destroy lock*/
	DestroyLock(lockNum);
	DestroyLock(-1);
	DestroyLock(55);
	DestroyLock(9000);





	/*Try to acquire, release amd destroy a destroyed lock*/
	Acquire(lockNum);
	Release(lockNum);
	DestroyLock(lockNum);

	Exit(1);
}